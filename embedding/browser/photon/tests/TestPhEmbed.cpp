/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Brian Edmond <briane@qnx.com>
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <Pt.h>
#include "PtMozilla.h"

PtWidget_t *window, *back, *forward, *stop, *web, *reload, *url, *status, *progress;
char *statusMessage = NULL;

/////////////////////////////////////////////////////////////////////////////
// callbacks for the main window controls
int reload_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtArg_t args[1];
	PtSetArg(&args[0], Pt_ARG_MOZ_RELOAD, 0, 0);
	PtSetResources(web, 1, args);
	return (Pt_CONTINUE);
}

int stop_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtArg_t args[1];
	PtSetArg(&args[0], Pt_ARG_MOZ_STOP, 0, 0);
	PtSetResources(web, 1, args);
	return (Pt_CONTINUE);
}

int back_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtArg_t args[1];
	PtSetArg(&args[0], Pt_ARG_MOZ_NAVIGATE_PAGE, WWW_DIRECTION_BACK, 0);
	PtSetResources(web, 1, args);
	return (Pt_CONTINUE);
}

int forward_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtArg_t args[1];
	PtSetArg(&args[0], Pt_ARG_MOZ_NAVIGATE_PAGE, WWW_DIRECTION_FWD, 0);
	PtSetResources(web, 1, args);
	return (Pt_CONTINUE);
}

int load_url_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtTextCallback_t *tcb = (PtTextCallback_t *)cbinfo->cbdata;
	PtArg_t args[1];

	PtSetArg(&args[0], Pt_ARG_MOZ_GET_URL, tcb->text, 0);
	PtSetResources(web, 1, args);
	return (Pt_CONTINUE);
}
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// mozilla widget callbacks


int moz_status_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaStatusCb_t *s = (PtMozillaStatusCb_t *) cbinfo->cbdata;
	PtArg_t args[2];
	struct BaseData *bd;

	switch (s->type)
	{
		case Pt_MOZ_STATUS_LINK:
		case Pt_MOZ_STATUS_JS:
			PtSetResource(status, Pt_ARG_TEXT_STRING, s->message, 0);
			break;
		case Pt_MOZ_STATUS_TITLE:
			PtSetResource(window, Pt_ARG_WINDOW_TITLE, s->message, 0);
			break;
	}

	return (Pt_CONTINUE);
}

int moz_start_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	// could start an animation here
	PtSetResource(stop, Pt_ARG_FLAGS, 0, Pt_BLOCKED|Pt_GHOST);

	return (Pt_CONTINUE);
}

int moz_complete_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtSetResource(status, Pt_ARG_TEXT_STRING, "Done", 0);

	// could stop an animation here
	PtSetResource(stop, Pt_ARG_FLAGS, ~0, Pt_BLOCKED|Pt_GHOST);

	return (Pt_CONTINUE);
}

int moz_progress_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaProgressCb_t *c = (PtMozillaProgressCb_t *) cbinfo->cbdata;
	PtArg_t args[2];
	int percent;
	char message[256];

	if ((c->max <= 0) || (c->cur > c->max))
	{
		percent = 0;
		sprintf(message, "%s (%d bytes loaded)", statusMessage, c->cur);
	}
	else
	{
		percent = (c->cur*100)/c->max;
		sprintf(message, "%s (%d%% complete, %d bytes of %d loaded)", statusMessage, percent, c->cur, c->max);
	}

	PtSetResource(progress, Pt_ARG_GAUGE_VALUE, percent, 0);
	PtSetResource(status, Pt_ARG_TEXT_STRING, message, 0);

	return (Pt_CONTINUE);
}

int moz_url_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	int *nflags = NULL;
	PtMozillaUrlCb_t *c = (PtMozillaUrlCb_t *) cbinfo->cbdata;
	struct BaseData *bd;

	// display the url in the entry field
	PtSetResource(url, Pt_ARG_TEXT_STRING, c->url, 0);

	/* get the navigation possibilities */
	PtGetResource(web, Pt_ARG_MOZ_NAVIGATE_PAGE, &nflags, 0 );

	if ( nflags != NULL ) 
	{
		// disable or enable the forward and back buttons accordingly
		if (*nflags & WWW_DIRECTION_BACK)
			PtSetResource(back, Pt_ARG_FLAGS, 0, Pt_BLOCKED|Pt_GHOST);
		else
			PtSetResource(back, Pt_ARG_FLAGS, ~0, Pt_BLOCKED|Pt_GHOST);
		if (*nflags & WWW_DIRECTION_FWD)
			PtSetResource(forward, Pt_ARG_FLAGS, 0, Pt_BLOCKED|Pt_GHOST);
		else
			PtSetResource(forward, Pt_ARG_FLAGS, ~0, Pt_BLOCKED|Pt_GHOST);
	}

	return (Pt_CONTINUE);
}

int moz_event_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtArg_t args[2];
	int *nflags = NULL;
	PtMozillaEventCb_t *c = (PtMozillaEventCb_t *) cbinfo->cbdata;

	printf("Callback: Event\n");

	return (Pt_CONTINUE);
}

int moz_net_state_change_cb (PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaNetStateCb_t *c = (PtMozillaNetStateCb_t *) cbinfo->cbdata;

	statusMessage = NULL;

	if (c->flags & PH_MOZ_EMBED_FLAG_IS_REQUEST) 
	{
		if (c->flags & PH_MOZ_EMBED_FLAG_REDIRECTING)
			statusMessage = "Redirecting to site...";
		else if (c->flags & PH_MOZ_EMBED_FLAG_TRANSFERRING)
			statusMessage = "Transferring data from site...";
		else if (c->flags & PH_MOZ_EMBED_FLAG_NEGOTIATING)
			statusMessage = "Waiting for authorization...";
	}

	if (c->status == PH_MOZ_EMBED_STATUS_FAILED_DNS)
		statusMessage = "Site not found.";
	else if (c->status == PH_MOZ_EMBED_STATUS_FAILED_CONNECT)
		statusMessage = "Failed to connect to site.";
	else if (c->status == PH_MOZ_EMBED_STATUS_FAILED_TIMEOUT)
		statusMessage = "Failed due to connection timeout.";
	else if (c->status == PH_MOZ_EMBED_STATUS_FAILED_USERCANCELED)
		statusMessage = "User canceled connecting to site.";

	if (c->flags & PH_MOZ_EMBED_FLAG_IS_WINDOW) 
	{
		if (c->flags & PH_MOZ_EMBED_FLAG_START)
	  		statusMessage = "Loading site...";
		else if (c->flags & PH_MOZ_EMBED_FLAG_STOP)
	  		statusMessage = "Done.";
	}

	if (statusMessage)
		PtSetResource(status, Pt_ARG_TEXT_STRING, statusMessage, 0);

	return (Pt_CONTINUE);
}

int moz_new_window_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaNewWindowCb_t *c = (PtMozillaNewWindowCb_t *) cbinfo->cbdata;

	printf("Callback: New window\n");

	return (Pt_CONTINUE);
}

int moz_resize_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaNewWindowCb_t *c = (PtMozillaNewWindowCb_t *) cbinfo->cbdata;

	printf("Callback: Resize\n");

	return (Pt_CONTINUE);
}

int moz_destroy_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaNewWindowCb_t *c = (PtMozillaNewWindowCb_t *) cbinfo->cbdata;

	printf("Callback: Destroy\n");

	return (Pt_CONTINUE);
}

int moz_visibility_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaNewWindowCb_t *c = (PtMozillaNewWindowCb_t *) cbinfo->cbdata;

	printf("Callback: Visibility\n");

	return (Pt_CONTINUE);
}

int moz_open_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtArg_t args[2];
	int *nflags = NULL;
	PtMozillaUrlCb_t *c = (PtMozillaUrlCb_t *) cbinfo->cbdata;

	// return Pt_END to prevent this page from loading

	//if (strcmp(c->url, "http://www.google.com/") == 0)
	//	return (Pt_END);

	printf("Callback: Open\n");

	return (Pt_CONTINUE);
}
/////////////////////////////////////////////////////////////////////////////

int
main(int argc, char **argv)
{
	int n = 0;
	PtArg_t args[10];
	PhDim_t	win_dim = {700, 700};
	PhArea_t area = {{0, 0}, {70, 25}};

	// main window creation
	PtSetArg(&args[n++], Pt_ARG_TITLE, "PtMozilla Test\n", 0);
	PtSetArg(&args[n++], Pt_ARG_DIM, &win_dim, 0);
  	window = PtAppInit(NULL, NULL, NULL, n, args);

	// back button
	n = 0;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Back", 0);
  	back = PtCreateWidget(PtButton, window, n, args);
  	PtAddCallback(back, Pt_CB_ACTIVATE, back_cb, NULL);

	// forward button
	n = 0;
	area.pos.x += 71;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Forward", 0);
  	forward = PtCreateWidget(PtButton, window, n, args);
  	PtAddCallback(forward, Pt_CB_ACTIVATE, forward_cb, NULL);

	// reload button
	n = 0;
	area.pos.x += 71;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Reload", 0);
  	reload = PtCreateWidget(PtButton, window, n, args);
  	PtAddCallback(reload, Pt_CB_ACTIVATE, reload_cb, NULL);

	// stop button
	n = 0;
	area.pos.x += 71;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Stop", 0);
  	stop = PtCreateWidget(PtButton, window, n, args);
  	PtAddCallback(stop, Pt_CB_ACTIVATE, stop_cb, NULL);

	// url entry field
	n = 0;
	area.pos.x += 71;
	area.size.w = win_dim.w - area.pos.x - 2;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
  	url = PtCreateWidget(PtText, window, n, args);
  	PtAddCallback(url, Pt_CB_ACTIVATE, load_url_cb, NULL);

	// status field
	n = 0;
	area.pos.x = 0;
	area.pos.y = win_dim.h - 22;
	area.size.w = win_dim.w - 100;
	area.size.h = 22;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
  	status = PtCreateWidget(PtLabel, window, n, args);
  	
	// progress bar
	n = 0;
	area.pos.x = area.size.w + 2;
	area.size.w = 97;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
  	progress = PtCreateWidget(PtProgress, window, n, args);

	n = 0;
	area.pos.y = 26;
	area.pos.x = 0;
	area.size.w = win_dim.w;
	area.size.h = win_dim.h - 26 - 22;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
  	web = PtCreateWidget(PtMozilla, window, n, args);
    PtAddCallback(web, Pt_CB_MOZ_STATUS, 		moz_status_cb, NULL);
    PtAddCallback(web, Pt_CB_MOZ_START, 		moz_start_cb, NULL);
    PtAddCallback(web, Pt_CB_MOZ_COMPLETE, 		moz_complete_cb, NULL);
	PtAddCallback(web, Pt_CB_MOZ_PROGRESS, 		moz_progress_cb, NULL);
    PtAddCallback(web, Pt_CB_MOZ_URL, 			moz_url_cb, NULL);
    PtAddCallback(web, Pt_CB_MOZ_EVENT, 		moz_event_cb, NULL);
    PtAddCallback(web, Pt_CB_MOZ_NET_STATE, 	moz_net_state_change_cb, NULL);
    PtAddCallback(web, Pt_CB_MOZ_NEW_WINDOW, 	moz_new_window_cb, NULL);
    PtAddCallback(web, Pt_CB_MOZ_RESIZE, 		moz_resize_cb, NULL);
    PtAddCallback(web, Pt_CB_MOZ_DESTROY, 		moz_destroy_cb, NULL);
    PtAddCallback(web, Pt_CB_MOZ_VISIBILITY, 	moz_visibility_cb, NULL);
    PtAddCallback(web, Pt_CB_MOZ_OPEN, 			moz_open_cb, NULL);

  	PtRealizeWidget(window);

  	PtMainLoop();
}
