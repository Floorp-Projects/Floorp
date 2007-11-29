/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2007 Chris Wilson
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Chris Wilson.
 *
 */

#include "cairoint.h"

#include "cairo-xlib-private.h"

#include <fontconfig/fontconfig.h>

#include <X11/Xlibint.h>	/* For XESetCloseDisplay */

typedef int (*cairo_xlib_error_func_t) (Display     *display,
					XErrorEvent *event);

struct _cairo_xlib_job {
    cairo_xlib_job_t *next;
    enum {
	RESOURCE,
	WORK
    } type;
    union {
	struct {
	    cairo_xlib_notify_resource_func notify;
	    XID xid;
	} resource;
	struct {
	    cairo_xlib_notify_func notify;
	    void *data;
	    void (*destroy) (void *);
	} work;
    } func;
};

static cairo_xlib_display_t *_cairo_xlib_display_list;

static void
_cairo_xlib_call_close_display_hooks (cairo_xlib_display_t *display)
{
    cairo_xlib_screen_info_t	    *screen;
    cairo_xlib_hook_t		    *hooks, *list;

    /* call all registered shutdown routines */
    CAIRO_MUTEX_LOCK (display->mutex);

    for (screen = display->screens; screen != NULL; screen = screen->next)
	_cairo_xlib_screen_info_close_display (screen);

    hooks = display->close_display_hooks;
    while (hooks != NULL) {
	display->close_display_hooks = NULL;
	CAIRO_MUTEX_UNLOCK (display->mutex);

	list = hooks;
	do {
	    cairo_xlib_hook_t *hook = list;
	    list = hook->next;

	    hook->func (display->display, hook->data);
	} while (list != NULL);

	CAIRO_MUTEX_LOCK (display->mutex);
	do {
	    cairo_xlib_hook_t *hook = hooks;
	    hooks = hook->next;

	    _cairo_freelist_free (&display->hook_freelist, hook);
	} while (hooks != NULL);

	hooks = display->close_display_hooks;
    }
    display->closed = TRUE;

    CAIRO_MUTEX_UNLOCK (display->mutex);
}

static void
_cairo_xlib_display_discard_screens (cairo_xlib_display_t *display)
{
    cairo_xlib_screen_info_t *screens;

    CAIRO_MUTEX_LOCK (display->mutex);
    screens = display->screens;
    display->screens = NULL;
    CAIRO_MUTEX_UNLOCK (display->mutex);

    while (screens != NULL) {
	cairo_xlib_screen_info_t *screen = screens;
	screens = screen->next;

	_cairo_xlib_screen_info_destroy (screen);
    }
}

cairo_xlib_display_t *
_cairo_xlib_display_reference (cairo_xlib_display_t *display)
{
    if (display == NULL)
	return NULL;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&display->ref_count));

    _cairo_reference_count_inc (&display->ref_count);

    return display;
}

void
_cairo_xlib_display_destroy (cairo_xlib_display_t *display)
{
    if (display == NULL)
	return;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&display->ref_count));

    if (! _cairo_reference_count_dec_and_test (&display->ref_count))
	return;

    /* destroy all outstanding notifies */
    while (display->workqueue != NULL) {
	cairo_xlib_job_t *job = display->workqueue;
	display->workqueue = job->next;

	if (job->type == WORK && job->func.work.destroy != NULL)
	    job->func.work.destroy (job->func.work.data);

	_cairo_freelist_free (&display->wq_freelist, job);
    }
    _cairo_freelist_fini (&display->wq_freelist);
    _cairo_freelist_fini (&display->hook_freelist);

    CAIRO_MUTEX_FINI (display->mutex);

    free (display);
}

static int
_noop_error_handler (Display     *display,
		     XErrorEvent *event)
{
    return False;		/* return value is ignored */
}
static int
_cairo_xlib_close_display (Display *dpy, XExtCodes *codes)
{
    cairo_xlib_display_t *display, **prev, *next;
    cairo_xlib_error_func_t old_handler;

    CAIRO_MUTEX_LOCK (_cairo_xlib_display_mutex);
    for (display = _cairo_xlib_display_list; display; display = display->next)
	if (display->display == dpy)
	    break;
    CAIRO_MUTEX_UNLOCK (_cairo_xlib_display_mutex);
    if (display == NULL)
	return 0;

    /* protect the notifies from triggering XErrors */
    XSync (dpy, False);
    old_handler = XSetErrorHandler (_noop_error_handler);

    _cairo_xlib_display_notify (display);
    _cairo_xlib_call_close_display_hooks (display);
    _cairo_xlib_display_discard_screens (display);

    /* catch any that arrived before marking the display as closed */
    _cairo_xlib_display_notify (display);

    XSync (dpy, False);
    XSetErrorHandler (old_handler);

    /*
     * Unhook from the global list
     */
    CAIRO_MUTEX_LOCK (_cairo_xlib_display_mutex);
    prev = &_cairo_xlib_display_list;
    for (display = _cairo_xlib_display_list; display; display = next) {
	next = display->next;
	if (display->display == dpy) {
	    *prev = next;
	    break;
	} else
	    prev = &display->next;
    }
    CAIRO_MUTEX_UNLOCK (_cairo_xlib_display_mutex);

    _cairo_xlib_display_destroy (display);

    /* Return value in accordance with requirements of
     * XESetCloseDisplay */
    return 0;
}

cairo_xlib_display_t *
_cairo_xlib_display_get (Display *dpy)
{
    cairo_xlib_display_t *display;
    cairo_xlib_display_t **prev;
    XExtCodes *codes;
    int major_unused, minor_unused;

    /* There is an apparent deadlock between this mutex and the
     * mutex for the display, but it's actually safe. For the
     * app to call XCloseDisplay() while any other thread is
     * inside this function would be an error in the logic
     * app, and the CloseDisplay hook is the only other place we
     * acquire this mutex.
     */
    CAIRO_MUTEX_LOCK (_cairo_xlib_display_mutex);

    for (prev = &_cairo_xlib_display_list; (display = *prev); prev = &(*prev)->next)
    {
	if (display->display == dpy) {
	    /*
	     * MRU the list
	     */
	    if (prev != &_cairo_xlib_display_list) {
		*prev = display->next;
		display->next = _cairo_xlib_display_list;
		_cairo_xlib_display_list = display;
	    }
	    break;
	}
    }

    if (display != NULL) {
	display = _cairo_xlib_display_reference (display);
	goto UNLOCK;
    }

    display = malloc (sizeof (cairo_xlib_display_t));
    if (display == NULL) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	goto UNLOCK;
    }

    /* Xlib calls out to the extension close_display hooks in LIFO
     * order. So we have to ensure that all extensions that we depend
     * on in our close_display hook are properly initialized before we
     * add our hook. For now, that means Render, so we call into its
     * QueryVersion function to ensure it gets initialized.
     */
    XRenderQueryVersion (dpy, &major_unused, &minor_unused);

    codes = XAddExtension (dpy);
    if (codes == NULL) {
	free (display);
	display = NULL;
	goto UNLOCK;
    }

    XESetCloseDisplay (dpy, codes->extension, _cairo_xlib_close_display);

    _cairo_freelist_init (&display->wq_freelist, sizeof (cairo_xlib_job_t));
    _cairo_freelist_init (&display->hook_freelist, sizeof (cairo_xlib_hook_t));

    CAIRO_REFERENCE_COUNT_INIT (&display->ref_count, 2); /* add one for the CloseDisplay */
    CAIRO_MUTEX_INIT (display->mutex);
    display->display = dpy;
    display->screens = NULL;
    display->workqueue = NULL;
    display->close_display_hooks = NULL;
    display->closed = FALSE;

    display->buggy_repeat = FALSE;
    if (strstr (ServerVendor (dpy), "X.Org") != NULL) {
	if (VendorRelease (dpy) <= 60802000)
	    display->buggy_repeat = TRUE;
    } else if (strstr (ServerVendor (dpy), "XFree86") != NULL) {
	if (VendorRelease (dpy) <= 40500000)
	    display->buggy_repeat = TRUE;
    }

    display->next = _cairo_xlib_display_list;
    _cairo_xlib_display_list = display;

UNLOCK:
    CAIRO_MUTEX_UNLOCK (_cairo_xlib_display_mutex);
    return display;
}

cairo_bool_t
_cairo_xlib_add_close_display_hook (Display *dpy, void (*func) (Display *, void *), void *data, const void *key)
{
    cairo_xlib_display_t *display;
    cairo_xlib_hook_t *hook;
    cairo_bool_t ret = FALSE;

    display = _cairo_xlib_display_get (dpy);
    if (display == NULL)
	return FALSE;

    CAIRO_MUTEX_LOCK (display->mutex);
    if (display->closed == FALSE) {
	hook = _cairo_freelist_alloc (&display->hook_freelist);
	if (hook != NULL) {
	    hook->func = func;
	    hook->data = data;
	    hook->key = key;

	    hook->next = display->close_display_hooks;
	    display->close_display_hooks = hook;
	    ret = TRUE;
	}
    }
    CAIRO_MUTEX_UNLOCK (display->mutex);

    _cairo_xlib_display_destroy (display);

    return ret;
}

void
_cairo_xlib_remove_close_display_hooks (Display *dpy, const void *key)
{
    cairo_xlib_display_t *display;
    cairo_xlib_hook_t *hook, *next, **prev;

    display = _cairo_xlib_display_get (dpy);
    if (display == NULL)
	return;

    CAIRO_MUTEX_LOCK (display->mutex);
    prev = &display->close_display_hooks;
    for (hook = display->close_display_hooks; hook != NULL; hook = next) {
	next = hook->next;
	if (hook->key == key) {
	    *prev = hook->next;
	    _cairo_freelist_free (&display->hook_freelist, hook);
	} else
	    prev = &hook->next;
    }
    *prev = NULL;
    CAIRO_MUTEX_UNLOCK (display->mutex);

    _cairo_xlib_display_destroy (display);
}

cairo_status_t
_cairo_xlib_display_queue_resource (cairo_xlib_display_t *display,
	                            cairo_xlib_notify_resource_func notify,
				    XID xid)
{
    cairo_xlib_job_t *job;
    cairo_status_t status = CAIRO_STATUS_NO_MEMORY;

    CAIRO_MUTEX_LOCK (display->mutex);
    if (display->closed == FALSE) {
	job = _cairo_freelist_alloc (&display->wq_freelist);
	if (job != NULL) {
	    job->type = RESOURCE;
	    job->func.resource.xid = xid;
	    job->func.resource.notify = notify;

	    job->next = display->workqueue;
	    display->workqueue = job;

	    status = CAIRO_STATUS_SUCCESS;
	}
    }
    CAIRO_MUTEX_UNLOCK (display->mutex);

    return status;
}

cairo_status_t
_cairo_xlib_display_queue_work (cairo_xlib_display_t *display,
	                        cairo_xlib_notify_func notify,
				void *data,
				void (*destroy) (void *))
{
    cairo_xlib_job_t *job;
    cairo_status_t status = CAIRO_STATUS_NO_MEMORY;

    CAIRO_MUTEX_LOCK (display->mutex);
    if (display->closed == FALSE) {
	job = _cairo_freelist_alloc (&display->wq_freelist);
	if (job != NULL) {
	    job->type = WORK;
	    job->func.work.data    = data;
	    job->func.work.notify  = notify;
	    job->func.work.destroy = destroy;

	    job->next = display->workqueue;
	    display->workqueue = job;

	    status = CAIRO_STATUS_SUCCESS;
	}
    }
    CAIRO_MUTEX_UNLOCK (display->mutex);

    return status;
}

void
_cairo_xlib_display_notify (cairo_xlib_display_t *display)
{
    cairo_xlib_job_t *jobs, *job, *freelist;
    Display *dpy = display->display;

    CAIRO_MUTEX_LOCK (display->mutex);
    jobs = display->workqueue;
    while (jobs != NULL) {
	display->workqueue = NULL;
	CAIRO_MUTEX_UNLOCK (display->mutex);

	/* reverse the list to obtain FIFO order */
	job = NULL;
	do {
	    cairo_xlib_job_t *next = jobs->next;
	    jobs->next = job;
	    job = jobs;
	    jobs = next;
	} while (jobs != NULL);
	freelist = jobs = job;

	do {
	    job = jobs;
	    jobs = job->next;

	    switch (job->type){
	    case WORK:
		job->func.work.notify (dpy, job->func.work.data);
		if (job->func.work.destroy != NULL)
		    job->func.work.destroy (job->func.work.data);
		break;

	    case RESOURCE:
		job->func.resource.notify (dpy, job->func.resource.xid);
		break;
	    }
	} while (jobs != NULL);

	CAIRO_MUTEX_LOCK (display->mutex);
	do {
	    job = freelist;
	    freelist = job->next;
	    _cairo_freelist_free (&display->wq_freelist, job);
	} while (freelist != NULL);

	jobs = display->workqueue;
    }
    CAIRO_MUTEX_UNLOCK (display->mutex);
}
