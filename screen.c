/*
 * screen.c - screen management
 *
 * Copyright © 2007 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "util.h"
#include "screen.h"
#include "tag.h"
#include "layout.h"
#include "focus.h"
#include "statusbar.h"

extern awesome_config globalconf;

/** Get screens info
 * \param screen Screen number
 * \param statusbar statusbar
 * \return ScreenInfo struct array with all screens info
 */
ScreenInfo *
get_screen_info(int screen, Statusbar *statusbar, Padding *padding)
{
    int i, screen_number = 0;
    ScreenInfo *si;

    if(XineramaIsActive(globalconf.display))
        si = XineramaQueryScreens(globalconf.display, &screen_number);
    else
    {
        /* emulate Xinerama info but only fill the screen we want */
        si = p_new(ScreenInfo, screen + 1);
        si[screen].width = DisplayWidth(globalconf.display, screen);
        si[screen].height = DisplayHeight(globalconf.display, screen);
        si[screen].x_org = 0;
        si[screen].y_org = 0;
        screen_number = screen + 1;
    }

     /* make padding corrections */
    if(padding)
    {
        si[screen].x_org += padding->left;
        si[screen].y_org += padding->top;
        si[screen].width -= padding->left + padding->right;
        si[screen].height -= padding->top + padding->bottom;
    }

    if(statusbar)
        for(i = 0; i < screen_number; i++)
            switch(statusbar->position)
            {
              case BarTop:
                si[i].y_org += statusbar->height;
              case BarBot:
                si[i].height -= statusbar->height;
                break;
              case BarLeft:
                si[i].x_org += statusbar->height;
              case BarRight:
                si[i].width -=  statusbar->height;
                break;
            }

    return si;
}

/** Get display info
 * \param screen Screen number
 * \param statusbar the statusbar
 * \return ScreenInfo struct pointer with all display info
 */
ScreenInfo *
get_display_info(int screen, Statusbar *statusbar, Padding *padding)
{
    ScreenInfo *si;

    si = p_new(ScreenInfo, 1);

    si->x_org = 0;
    si->y_org = statusbar && statusbar->position == BarTop ? statusbar->height : 0;
    si->width = DisplayWidth(globalconf.display, screen);
    si->height = DisplayHeight(globalconf.display, screen) -
        (statusbar && (statusbar->position == BarTop || statusbar->position == BarBot) ? statusbar->height : 0);

	/* make padding corrections */
	if(padding)
	{
		si[screen].x_org+=padding->left;
		si[screen].y_org+=padding->top;
		si[screen].width-=padding->left+padding->right;
		si[screen].height-=padding->top+padding->bottom;
	}

    return si;
}

/** Return the Xinerama screen number where the coordinates belongs to
 * \param x x coordinate of the window
 * \param y y coordinate of the window
 * \return screen number or DefaultScreen of disp on no match
 */
int
get_screen_bycoord(int x, int y)
{
    ScreenInfo *si;
    int i;

    /* don't waste our time */
    if(!XineramaIsActive(globalconf.display))
        return DefaultScreen(globalconf.display);

    si = get_screen_info(0, NULL, NULL);

    for(i = 0; i < get_screen_count(); i++)
        if((x < 0 || (x >= si[i].x_org && x < si[i].x_org + si[i].width))
           && (y < 0 || (y >= si[i].y_org && y < si[i].y_org + si[i].height)))
        {
            p_delete(&si);
            return i;
        }

    p_delete(&si);
    return DefaultScreen(globalconf.display);
}

/** Return the actual screen count
 * \return the number of screen available
 */
int
get_screen_count(void)
{
    int screen_number;

    if(XineramaIsActive(globalconf.display))
        XineramaQueryScreens(globalconf.display, &screen_number);
    else
        return ScreenCount(globalconf.display);

    return screen_number;
}

/** This returns the real X screen number for a logical
 * screen if Xinerama is active.
 * \param screen the logical screen
 * \return the X screen
 */
int
get_phys_screen(int screen)
{
    if(XineramaIsActive(globalconf.display))
        return DefaultScreen(globalconf.display);
    return screen;
}

/** Move a client to a virtual screen
 * \param c the client
 * \param doresize set to True if we also move the client to the new x_org and
 *         y_org of the new screen
 */
void
move_client_to_screen(Client *c, int new_screen, Bool doresize)
{
    Tag *tag;
    int old_screen = c->screen;

    for(tag = globalconf.screens[old_screen].tags; tag; tag = tag->next)
        untag_client(c, tag, old_screen);

    /* tag client with new screen tags */
    tag_client_with_current_selected(c, new_screen);

    c->screen = new_screen;

    if(doresize && old_screen != c->screen)
    {
        ScreenInfo *si, *si_old;

        si = get_screen_info(c->screen, NULL, NULL);
        si_old = get_screen_info(old_screen, NULL, NULL);

        /* compute new coords in new screen */
        c->rx = (c->rx - si_old[old_screen].x_org) + si[c->screen].x_org;
        c->ry = (c->ry - si_old[old_screen].y_org) + si[c->screen].y_org;
        /* check that new coords are still in the screen */
        if(c->rw > si[c->screen].width)
            c->rw = si[c->screen].width;
        if(c->rh > si[c->screen].height)
            c->rh = si[c->screen].height;
        if(c->rx + c->rw >= si[c->screen].x_org + si[c->screen].width)
            c->rx = si[c->screen].x_org + si[c->screen].width - c->rw - 2 * c->border;
        if(c->ry + c->rh >= si[c->screen].y_org + si[c->screen].height)
            c->ry = si[c->screen].y_org + si[c->screen].height - c->rh - 2 * c->border;

        client_resize(c, c->rx, c->ry, c->rw, c->rh, True, False);

        p_delete(&si);
        p_delete(&si_old);
    }

    focus(c, True, c->screen);

    /* redraw statusbar on all screens */
    statusbar_draw(old_screen);
    statusbar_draw(new_screen);
}

/** Move mouse pointer to x_org and y_xorg of specified screen
 * \param screen screen number
 */
static void
move_mouse_pointer_to_screen(int screen)
{
    if(XineramaIsActive(globalconf.display))
    {
        ScreenInfo *si = get_screen_info(screen, NULL, NULL);
        XWarpPointer(globalconf.display, None, DefaultRootWindow(globalconf.display), 0, 0, 0, 0, si[screen].x_org, si[screen].y_org);
        p_delete(&si);
    }
    else
        XWarpPointer(globalconf.display, None, RootWindow(globalconf.display, screen), 0, 0, 0, 0, 0, 0);
}


/** Switch focus to a specified screen
 * \param arg screen number
 * \ingroup ui_callback
 */
void
uicb_screen_focus(int screen, char *arg)
{
    int new_screen, numscreens = get_screen_count();

    if(arg)
        new_screen = compute_new_value_from_arg(arg, screen);
    else
        new_screen = screen + 1;

    if (new_screen < 0)
        new_screen = numscreens - 1;
    if (new_screen > (numscreens - 1))
        new_screen = 0;

    focus(focus_get_latest_client_for_tag(new_screen, get_current_tag(new_screen)),
          True, new_screen);

    move_mouse_pointer_to_screen(new_screen);
}

/** Move client to a virtual screen (if Xinerama is active)
 * \param arg screen number
 * \ingroup ui_callback
 */
void
uicb_client_movetoscreen(int screen __attribute__ ((unused)), char *arg)
{
    int new_screen, prev_screen;
    Client *sel = globalconf.focus->client;

    if(!sel || !XineramaIsActive(globalconf.display))
        return;

    if(arg)
        new_screen = compute_new_value_from_arg(arg, sel->screen);
    else
        new_screen = sel->screen + 1;

    if(new_screen >= get_screen_count())
        new_screen = 0;
    else if(new_screen < 0)
        new_screen = get_screen_count() - 1;

    prev_screen = sel->screen;
    move_client_to_screen(sel, new_screen, True);
    move_mouse_pointer_to_screen(new_screen);
    arrange(prev_screen);
    arrange(new_screen);
}
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99
