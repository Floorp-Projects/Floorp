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
#include <photon/PtWebClient.h>
#include <photon/PtProgress.h>
#include "../src/PtMozilla.h"

int window_count = 0;
struct window_info
{
	PtWidget_t *window, *back, *forward, *stop, *web, *reload, *url, *status, \
		*progress, *print, *sel, *save, *stream;
	char *statusMessage;
};


PtWidget_t *create_browser_window(unsigned window_flags);

/////////////////// modal authentication dialog /////////////////////////////
struct Auth_data
{
	PtWidget_t *awin, *auser, *apass, *aok, *acan;
	PtModalCtrl_t mc;
	char *user, *pass;
};

// retrieve user input from authorization dialog
int dlg_auth_cb( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
    struct Auth_data *auth = (struct Auth_data *) data;
	char *u, *p;

    // check which widget triggered the callback
    if (widget == auth->aok)
	{
		PtGetResource(auth->auser, Pt_ARG_TEXT_STRING, &u, 0);
		PtGetResource(auth->apass, Pt_ARG_TEXT_STRING, &p, 0);
		auth->user = strdup(u);
		auth->pass = strdup(p);
    }
    
	PtModalUnblock(&(auth->mc), (int *)1);

    return (Pt_CONTINUE);
}

int CreateAuthWindow(PtMozillaAuthenticateCb_t *acb, PtWidget_t *parent)
{
	PhArea_t area = {{0, 0}, {310, 100}};
	PtArg_t args[5];
	struct Auth_data *auth = (struct Auth_data *)calloc (sizeof(struct Auth_data), 1);
	int response;

	// window
	PtSetArg(&args[0], Pt_ARG_DIM, &(area.size), 0);
	PtSetArg(&args[1], Pt_ARG_WINDOW_TITLE, "Authentication", 0);
	auth->awin = PtCreateWidget(PtWindow, parent, 2, args);

	// labels
	area.pos.x = 7;
	area.pos.y = 7;
	PtSetArg(&args[0], Pt_ARG_POS, &area.pos, 0);
	PtSetArg(&args[1], Pt_ARG_TEXT_STRING, "Username:", 0);
	PtCreateWidget(PtLabel, auth->awin, 2, args);
	area.pos.y += 30;
	PtSetArg(&args[0], Pt_ARG_POS, &area.pos, 0);
	PtSetArg(&args[1], Pt_ARG_TEXT_STRING, "Password:", 0);
	PtCreateWidget(PtLabel, auth->awin, 2, args);

	// fields
	area.pos.x = 90;
	area.pos.y = 5;
	area.size.w = 210;
	area.size.h = 20;
	PtSetArg(&args[0], Pt_ARG_AREA, &area, 0);
	auth->auser = PtCreateWidget(PtText, auth->awin, 1, args);
	area.pos.y += 30;
	PtSetArg(&args[0], Pt_ARG_AREA, &area, 0);
	auth->apass = PtCreateWidget(PtText, auth->awin, 1, args);

	// buttons
	area.pos.x = 155;
	area.pos.y = 68;
	area.size.w = 70;
	area.size.h = 21;
	PtSetArg(&args[0], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[1], Pt_ARG_TEXT_STRING, "Ok", 0);
	auth->aok = PtCreateWidget(PtButton, auth->awin, 2, args);
	PtAddCallback(auth->aok, Pt_CB_ACTIVATE, dlg_auth_cb, auth);
	area.pos.x += 75;
	PtSetArg(&args[0], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[1], Pt_ARG_TEXT_STRING, "Cancel", 0);
	auth->acan = PtCreateWidget(PtButton, auth->awin, 2, args);
	PtAddCallback(auth->acan, Pt_CB_ACTIVATE, dlg_auth_cb, auth);

	PtRealizeWidget(auth->awin);

	response = (int) PtModalBlock(&(auth->mc), 0);
	PtDestroyWidget(auth->awin);
	if (auth->user && auth->pass)
	{
		strcpy(acb->user, auth->user);
		strcpy(acb->pass, auth->pass);
		free(auth->user);
		free(auth->pass);
		free(auth);
		return (Pt_CONTINUE);
	}

	free(auth);
	return (Pt_END);
}
//////////////////////////////////////////////////////////////////////////////

/////////////// callbacks for the main window controls ///////////////////////

int window_close_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	window_count--;
	if (window_count == 0)
		exit (0);
	return (Pt_CONTINUE);
}

int reload_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtArg_t args[1];
	struct window_info *info;
	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &info, 0);
	PtSetArg(&args[0], Pt_ARG_MOZ_RELOAD, 0, 0);
	PtSetResources(info->web, 1, args);
	return (Pt_CONTINUE);
}

int print_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
    PtArg_t args[1];
	struct window_info *info;
	PpPrintContext_t *pc = NULL;

	pc = PpCreatePC();

	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &info, 0);
	if (PtPrintSelection(info->window, NULL, "Print", pc, Pt_PRINTSEL_DFLT_LOOK) != Pt_PRINTSEL_CANCEL)
	{
		PtSetArg(&args[0], Pt_ARG_MOZ_PRINT, pc, 0);
		PtSetResources(info->web, 1, args);
	}
	PpReleasePC(pc);

    return (Pt_CONTINUE);
}

int stop_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtArg_t args[1];
	struct window_info *info;
	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &info, 0);
	PtSetArg(&args[0], Pt_ARG_MOZ_STOP, 0, 0);
	PtSetResources(info->web, 1, args);
	return (Pt_CONTINUE);
}

int back_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtArg_t args[1];
	struct window_info *info;
	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &info, 0);
	PtSetArg(&args[0], Pt_ARG_MOZ_NAVIGATE_PAGE, WWW_DIRECTION_BACK, 0);
	PtSetResources(info->web, 1, args);
	return (Pt_CONTINUE);
}

int forward_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtArg_t args[1];
	struct window_info *info;
	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &info, 0);
	PtSetArg(&args[0], Pt_ARG_MOZ_NAVIGATE_PAGE, WWW_DIRECTION_FWD, 0);
	PtSetResources(info->web, 1, args);
	return (Pt_CONTINUE);
}

int load_url_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtTextCallback_t *tcb = (PtTextCallback_t *)cbinfo->cbdata;
	PtArg_t args[1];
	struct window_info *info;
	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &info, 0);

	PtSetArg(&args[0], Pt_ARG_MOZ_GET_URL, tcb->text, 0);
	PtSetResources(info->web, 1, args);
	return (Pt_CONTINUE);
}

int save_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	struct window_info *info;
	PtFileSelectionInfo_t i;
	char *home = getenv("HOME");

	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &info, 0);

	memset (&i, 0, sizeof(PtFileSelectionInfo_t));
	PtFileSelection(info->window, NULL, "Save Page As", home, "*.html", \
		NULL, NULL, NULL, &i, Pt_FSR_NO_FCHECK);

	MozSavePageAs(info->web, i.path, Pt_MOZ_SAVEAS_HTML);
	
	return (Pt_CONTINUE);
}


/////////////////////////////////////////////////////////////////////////////

//////////////////// mozilla widget callbacks ///////////////////////////////
int moz_info_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaInfoCb_t *info = (PtMozillaInfoCb_t *) cbinfo->cbdata;
	PtMozillaInfoCb_t *s = (PtMozillaInfoCb_t *) cbinfo->cbdata;
	struct window_info *i;
	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &i, 0);

	switch (s->type)
	{
		case Pt_MOZ_INFO_LINK:
		case Pt_MOZ_INFO_JSSTATUS:
			if (i->status)
				PtSetResource(i->status, Pt_ARG_TEXT_STRING, info->data, 0);
			break;
		case Pt_MOZ_INFO_TITLE:
			PtSetResource(i->window, Pt_ARG_WINDOW_TITLE, info->data, 0);
			break;
		case Pt_MOZ_INFO_SSL:
			printf("SSL status: ");
			if (info->status & Pt_SSL_STATE_IS_INSECURE)
				printf("INSECURE ");
			if (info->status & Pt_SSL_STATE_IS_BROKEN)
				printf("BROKEN ");
			if (info->status & Pt_SSL_STATE_IS_SECURE)
				printf("SECURE ");
			if (info->status & Pt_SSL_STATE_SECURE_HIGH)
				printf("HIGH ");
			if (info->status & Pt_SSL_STATE_SECURE_MED)
				printf("MED ");
			if (info->status & Pt_SSL_STATE_SECURE_LOW)
				printf("LOW ");
			printf("\n");
			break;
	}

	return (Pt_CONTINUE);
}

int moz_start_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	struct window_info *i;
	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &i, 0);
	// could start an animation here
	if (i->status)
		PtSetResource(i->status, Pt_ARG_TEXT_STRING, "Starting to load page", 0);
	if (i->stop)
		PtSetResource(i->stop, Pt_ARG_FLAGS, 0, Pt_BLOCKED|Pt_GHOST);

	return (Pt_CONTINUE);
}

int moz_complete_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	struct window_info *i;

	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &i, 0);
	if (i->status) 
		PtSetResource(i->status, Pt_ARG_TEXT_STRING, "Done", 0);

	// could stop an animation here
	if (i->stop)
		PtSetResource(i->stop, Pt_ARG_FLAGS, ~0, Pt_BLOCKED|Pt_GHOST);

	return (Pt_CONTINUE);
}

int moz_progress_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaProgressCb_t *c = (PtMozillaProgressCb_t *) cbinfo->cbdata;
	int percent;
	char message[256]="";
	struct window_info *i;

	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &i, 0);

	if ((c->max <= 0) || (c->cur > c->max))
	{
   		percent = 100;
    	//if ( i->statusMessage != NULL ) 
        	//sprintf(message, "%s (%d bytes loaded)", i->statusMessage, c->cur);
        	sprintf(message, "%s (%d bytes loaded)", "Loading", c->cur);
	}
	else
	{ 
		percent = (c->cur*100)/c->max;
    	//if ( i->statusMessage != NULL ) 
        	//sprintf(message, "%s (%d%% complete, %d bytes of %d loaded)", i->statusMessage, percent, c->cur, c->max);
        	sprintf(message, "%s (%d%% complete, %d bytes of %d loaded)", "Transferring", percent, c->cur, c->max);
	}
#if 0 
printf ("Progress: %d\n",(int)percent);
#endif

	if (i->progress) 
		PtSetResource(i->progress, Pt_ARG_GAUGE_VALUE, percent, 0);
	if (i->status)
		PtSetResource(i->status, Pt_ARG_TEXT_STRING, message, 0);

	return (Pt_CONTINUE);
}

int moz_url_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	int *nflags = NULL;
	PtMozillaUrlCb_t *c = (PtMozillaUrlCb_t *) cbinfo->cbdata;
	struct window_info *i;
	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &i, 0);

	// display the url in the entry field
	if (i->url)
		PtSetResource(i->url, Pt_ARG_TEXT_STRING, c->url, 0);

	/* get the navigation possibilities */
	PtGetResource(i->web, Pt_ARG_MOZ_NAVIGATE_PAGE, &nflags, 0 );

	if ( nflags != NULL ) 
	{
		// disable or enable the forward and back buttons accordingly
		if (i->back)
		{
			if (*nflags & (1 << WWW_DIRECTION_BACK))
				PtSetResource(i->back, Pt_ARG_FLAGS, 0, Pt_BLOCKED|Pt_GHOST);
			else
				PtSetResource(i->back, Pt_ARG_FLAGS, ~0, Pt_BLOCKED|Pt_GHOST);
		}
		if (i->forward)
		{
			if (*nflags & (1 << WWW_DIRECTION_FWD))
				PtSetResource(i->forward, Pt_ARG_FLAGS, 0, Pt_BLOCKED|Pt_GHOST);
			else
				PtSetResource(i->forward, Pt_ARG_FLAGS, ~0, Pt_BLOCKED|Pt_GHOST);
		}
	}

	return (Pt_CONTINUE);
}

int moz_net_state_change_cb (PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
#if 0
	PtMozillaNetStateCb_t *c = (PtMozillaNetStateCb_t *) cbinfo->cbdata;
	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &i, 0);

	i->statusMessage = NULL;

	if (c->flags & STATE_REQUEST) 
	{
		if (c->flags & STATE_REDIRECTING)
			statusMessage = "Redirecting to site...";
		else if (c->flags & STATE_TRANSFERRING)
			statusMessage = "Transferring data from site...";
		else if (c->flags & STATE_NEGOTIATING)
			statusMessage = "Waiting for authorization...";
	}

	if (c->status == STATEFAILED_DNS)
		statusMessage = "Site not found.";
	else if (c->status == STATEFAILED_CONNECT)
		statusMessage = "Failed to connect to site.";
	else if (c->status == STATEFAILED_TIMEOUT)
		statusMessage = "Failed due to connection timeout.";
	else if (c->status == STATEFAILED_USERCANCELED)
		statusMessage = "User canceled connecting to site.";

	if (c->flags & STATE_WINDOW) 
	{
		if (c->flags & STATE_START)
	  		statusMessage = "Loading site...";
		else if (c->flags & STATE_STOP)
	  		statusMessage = "Done.";
	}

	if (statusMessage)
		PtSetResource(status, Pt_ARG_TEXT_STRING, statusMessage, 0);

#endif
	return (Pt_CONTINUE);
}

int moz_new_window_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaNewWindowCb_t *c = (PtMozillaNewWindowCb_t *) cbinfo->cbdata;
	struct window_info *i;
	PtWidget_t *win;

	win = create_browser_window(c->window_flags);
	if (win)
	{
		PtGetResource(win, Pt_ARG_POINTER, &i, 0);
		c->widget = i->web;
		return (Pt_CONTINUE);
	}

	return (Pt_END);
}

int moz_visibility_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaVisibilityCb_t *v = (PtMozillaVisibilityCb_t *) cbinfo->cbdata;
	struct window_info *i;

	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &i, 0);
	if (v->show)
  		PtRealizeWidget(i->window);
  	else
  		PtUnrealizeWidget(i->window);

	return (Pt_CONTINUE);
}
int moz_open_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
		return (Pt_CONTINUE);
}

int moz_auth_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaAuthenticateCb_t *a = (PtMozillaAuthenticateCb_t *) cbinfo->cbdata;

	return (CreateAuthWindow(a, PtFindDisjoint(widget)));
}

int moz_dialog_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaDialogCb_t *d = (PtMozillaDialogCb_t *) cbinfo->cbdata;

	switch (d->type)
	{
		case Pt_MOZ_DIALOG_ALERT:
			PtAskQuestion(NULL, "JS Alert", (d->text) ? d->text : "Alert Message.", \
				NULL, "OK", NULL, NULL, 1);
			break;
		case Pt_MOZ_DIALOG_ALERT_CHECK:
			printf("Alert Check\n");
			printf("\tMessage: %s\n", d->message);
			break;
		case Pt_MOZ_DIALOG_CONFIRM:
			if (PtAskQuestion(NULL, "JS Confirm", (d->text) ? d->text : "Confirm Message.", \
				NULL, "Yes", "No", NULL, 1) == 1)
			{
				return (Pt_CONTINUE);
			}
			else
			{
				return (Pt_END);
			}
			break;
		case Pt_MOZ_DIALOG_CONFIRM_CHECK:
			printf("Confirm Check\n");
			printf("\tMessage: %s\n", d->message);
			break;
	}

	return (Pt_END);
}

int moz_prompt_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaPromptCb_t *p = (PtMozillaPromptCb_t *) cbinfo->cbdata;
	int answer;
	char *btns[] = { "&Ok", "&Cancel" };
	char text[128];
	int len = 127;
	struct window_info *i;
	PtGetResource(PtFindDisjoint(widget), Pt_ARG_POINTER, &i, 0);

	if (p->dflt_resp)
		strcpy(text, p->dflt_resp);

	answer = PtPrompt(i->window, NULL, "User Prompt", NULL, (p->text) ? p->text : "Prompt Message.", \
				NULL, 2, (char const **)btns, NULL, 1, 2, len, text, NULL, NULL, 0 );

	switch( answer ) 
	{
		case 1: // ok
			strcpy(p->response, text);
			return (Pt_CONTINUE);
			break;
		case 2: // cancel
		default:
			return (Pt_END);
			break;
	}

	return (Pt_END);
}

int moz_destroy_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtDestroyWidget(PtFindDisjoint(widget));
	return (Pt_CONTINUE);
}

int moz_context_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaContextCb_t *c = (PtMozillaContextCb_t *) cbinfo->cbdata;

	printf("Context Callback:\n");
	if (c->flags & Pt_MOZ_CONTEXT_LINK)
		printf("\tLINK\n");
	if (c->flags & Pt_MOZ_CONTEXT_IMAGE)
		printf("\tIMAGE\n");
	if (c->flags & Pt_MOZ_CONTEXT_DOCUMENT)
		printf("\tDOCUMENT\n");
	if (c->flags & Pt_MOZ_CONTEXT_TEXT)
		printf("\tTEXT\n");
	if (c->flags & Pt_MOZ_CONTEXT_INPUT)
		printf("\tINPUT\n");

	return (Pt_CONTINUE);
}

int moz_print_status_cb(PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
	PtMozillaPrintStatusCb_t *c = (PtMozillaPrintStatusCb_t *) cbinfo->cbdata;

	switch (c->status)
	{
		case Pt_MOZ_PRINT_START:
			printf("Printing: start\n");
			break;
		case Pt_MOZ_PRINT_COMPLETE:
			printf("Printing: complete\n");
			break;
		case Pt_MOZ_PRINT_PROGRESS:
			printf("Printing: progress %d-%d\n", c->cur, c->max);
			break;
	}

	return (Pt_CONTINUE);
}
/////////////////////////////////////////////////////////////////////////////


///////////////////////////////// create a main window //////////////////////
PtWidget_t *create_browser_window(unsigned window_flags)
{
	int n = 0;
	PtArg_t args[10];
	PhDim_t	win_dim = {750, 700};
	PhDim_t	dim = {0, 0}, btn_dim = {70, 25};
	PhArea_t area = {{0, 0}, {70, 25}};
	struct window_info	*info = (struct window_info *) calloc (sizeof(struct window_info), 1);
	PtWidget_t *w, *container;
	int render = 0;

	// main window creation
	PtSetArg(&args[n++], Pt_ARG_TITLE, "PtMozilla Test\n", 0);
	PtSetArg(&args[n++], Pt_ARG_DIM, &win_dim, 0);
	PtSetArg(&args[n++], Pt_ARG_POINTER, info, 0);

	if (window_flags & Pt_MOZ_FLAG_WINDOWBORDERSON)
		render |= Ph_WM_RENDER_BORDER;
	if (window_flags & Pt_MOZ_FLAG_WINDOWCLOSEON)
		render |= Ph_WM_RENDER_CLOSE;
	if (window_flags & Pt_MOZ_FLAG_WINDOWRESIZEON)
		render |= Ph_WM_RENDER_RESIZE | Ph_WM_RENDER_MAX;
	if (window_flags & Pt_MOZ_FLAG_TITLEBARON)
		render |= Ph_WM_RENDER_TITLE;

	PtSetArg(&args[n++], Pt_ARG_WINDOW_RENDER_FLAGS, ~0, render);
  	info->window = PtCreateWidget(PtWindow, NULL, n, args);
  	PtAddCallback(info->window, Pt_CB_WINDOW_CLOSING, window_close_cb, NULL);

	if (window_flags & Pt_MOZ_FLAG_TOOLBARON)
	{
		// Button toolbar
		n = 0;
		area.size.w = win_dim.w;
		area.size.h = btn_dim.h + 4;
		PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
		PtSetArg(&args[n++], Pt_ARG_ANCHOR_FLAGS, ~0, Pt_RIGHT_ANCHORED_RIGHT|Pt_LEFT_ANCHORED_LEFT|Pt_TOP_ANCHORED_TOP);
		container = PtCreateWidget(PtToolbar, info->window, n, args);
		
		// back button
		n = 0;
		PtSetArg(&args[n++], Pt_ARG_DIM, &btn_dim, 0);
		PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Back", 0);
		info->back = PtCreateWidget(PtButton, container, n, args);
		PtAddCallback(info->back, Pt_CB_ACTIVATE, back_cb, NULL);

		// forward button
		n = 0;
		area.pos.x += 71;
		PtSetArg(&args[n++], Pt_ARG_DIM, &btn_dim, 0);
		PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Forward", 0);
		info->forward = PtCreateWidget(PtButton, container, n, args);
		PtAddCallback(info->forward, Pt_CB_ACTIVATE, forward_cb, NULL);

		// reload button
		n = 0;
		area.pos.x += 71;
		PtSetArg(&args[n++], Pt_ARG_DIM, &btn_dim, 0);
		PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Reload", 0);
		info->reload = PtCreateWidget(PtButton, container, n, args);
		PtAddCallback(info->reload, Pt_CB_ACTIVATE, reload_cb, NULL);

		// stop button
		n = 0;
		area.pos.x += 71;
		PtSetArg(&args[n++], Pt_ARG_DIM, &btn_dim, 0);
		PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Stop", 0);
		info->stop = PtCreateWidget(PtButton, container, n, args);
		PtAddCallback(info->stop, Pt_CB_ACTIVATE, stop_cb, NULL);

		// print button
		n = 0;
		area.pos.x += 71;
		PtSetArg(&args[n++], Pt_ARG_DIM, &btn_dim, 0);
		PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Print", 0);
		info->print = PtCreateWidget(PtButton, container, n, args);
		PtAddCallback(info->print, Pt_CB_ACTIVATE, print_cb, NULL);

		// save as button
		n = 0;
		area.pos.x += 71;
		PtSetArg(&args[n++], Pt_ARG_DIM, &btn_dim, 0);
		PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Save", 0);
		info->save = PtCreateWidget(PtButton, container, n, args);
		PtAddCallback(info->save, Pt_CB_ACTIVATE, save_cb, NULL);
	
		PtExtentWidget (container);
		PtWidgetDim(container, &dim);	
		area.pos.y += dim.h + 1;
  	}

	if (window_flags & Pt_MOZ_FLAG_LOCATIONBARON)
	{
		// Location toolbar
		n = 0;
		area.pos.x = 0;
		area.size.w = win_dim.w;
		area.size.h = btn_dim.h + 4;
		PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
		PtSetArg(&args[n++], Pt_ARG_ANCHOR_FLAGS, ~0, Pt_RIGHT_ANCHORED_RIGHT|Pt_LEFT_ANCHORED_LEFT|Pt_TOP_ANCHORED_TOP);
		container = PtCreateWidget(PtToolbar, info->window, n, args);
		
		n = 0;
		PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "URL:", 0);
		w = PtCreateWidget(PtLabel, container, n, args);

		PtExtentWidget (w);
		PtWidgetDim(w, &dim);

		// url entry field
		n = 0;
		area.size.w = win_dim.w - 100;
		area.size.h = 22;
		PtSetArg(&args[n++], Pt_ARG_DIM, &area.size, 0);
		PtSetArg(&args[n++], Pt_ARG_ANCHOR_FLAGS, ~0, Pt_RIGHT_ANCHORED_RIGHT|Pt_LEFT_ANCHORED_LEFT|Pt_TOP_ANCHORED_TOP);
		info->url = PtCreateWidget(PtText, container, n, args);
		PtAddCallback(info->url, Pt_CB_ACTIVATE, load_url_cb, NULL);

		PtExtentWidget(container);
		PtWidgetDim(container, &dim);	
		area.pos.y += dim.h + 1;
  	}

	// PtMozilla container and widget
	n = 0;
	area.pos.x = 0;
	area.size.w = win_dim.w;
	area.size.h = win_dim.h - area.pos.y - btn_dim.h - 5; 
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[n++], Pt_ARG_ANCHOR_FLAGS, ~0, Pt_RIGHT_ANCHORED_RIGHT | Pt_LEFT_ANCHORED_LEFT | Pt_TOP_ANCHORED_TOP | Pt_BOTTOM_ANCHORED_BOTTOM);
	PtSetArg(&args[n++], Pt_ARG_POINTER, info, 0);
  	container = PtCreateWidget(PtContainer, info->window, n, args);

	n = 0;
	area.pos.x = 0;
	area.pos.y = 0;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[n++], Pt_ARG_ANCHOR_FLAGS, ~0, Pt_RIGHT_ANCHORED_RIGHT | Pt_LEFT_ANCHORED_LEFT | Pt_TOP_ANCHORED_TOP | Pt_BOTTOM_ANCHORED_BOTTOM);
  	info->web = PtCreateWidget(PtMozilla, container, n, args);

	PtExtentWidget (container);
	PtWidgetArea(container, &area);	
	area.pos.y += area.size.h + 1;

	if (window_flags & Pt_MOZ_FLAG_STATUSBARON)
	{
		// Status toolbar
		n = 0;
		area.pos.x = 0;
		area.size.w = win_dim.w;
		area.size.h = btn_dim.h + 3;
		PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
		PtSetArg(&args[n++], Pt_ARG_TOOLBAR_FLAGS, Pt_TOOLBAR_END_SEPARATOR|Pt_TOOLBAR_REVERSE_LAST_ITEM, Pt_TOOLBAR_END_SEPARATOR|Pt_TOOLBAR_REVERSE_LAST_ITEM|Pt_TOOLBAR_DRAGGABLE);
		PtSetArg(&args[n++], Pt_ARG_ANCHOR_FLAGS, ~0, Pt_RIGHT_ANCHORED_RIGHT|Pt_LEFT_ANCHORED_LEFT|Pt_TOP_ANCHORED_TOP);
		container = PtCreateWidget(PtToolbar, info->window, n, args);

		// status field
		n = 0;
		area.pos.x = 0;
		area.size.w = win_dim.w - 100;
		area.size.h = 22;
		PtSetArg(&args[n++], Pt_ARG_DIM, &area.size, 0);
		PtSetArg(&args[n++], Pt_ARG_FLAGS, Pt_HIGHLIGHTED, Pt_HIGHLIGHTED);
		PtSetArg(&args[n++], Pt_ARG_ANCHOR_FLAGS, ~0, Pt_RIGHT_ANCHORED_RIGHT|Pt_LEFT_ANCHORED_LEFT| \
				Pt_TOP_ANCHORED_BOTTOM);
		info->status = PtCreateWidget(PtLabel, container, n, args);
  	
		// progress bar
		n = 0;
		area.size.w = 75;
		PtSetArg(&args[n++], Pt_ARG_DIM, &area.size, 0);
		PtSetArg(&args[n++], Pt_ARG_ANCHOR_FLAGS, ~0, Pt_RIGHT_ANCHORED_RIGHT|Pt_LEFT_ANCHORED_RIGHT| \
				Pt_TOP_ANCHORED_BOTTOM);
		PtSetArg( &args[n++], Pt_ARG_GAUGE_FLAGS, Pt_GAUGE_LIVE, Pt_GAUGE_LIVE );
		info->progress = PtCreateWidget(PtProgress, container, n, args);
  	}

	PtAddCallback(info->web, Pt_CB_MOZ_PROGRESS, 		moz_progress_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_START, 		moz_start_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_COMPLETE, 		moz_complete_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_URL, 			moz_url_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_OPEN, 			moz_open_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_INFO,	 		moz_info_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_NET_STATE, 	moz_net_state_change_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_AUTHENTICATE, 	moz_auth_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_DIALOG, 		moz_dialog_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_PROMPT, 		moz_prompt_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_NEW_WINDOW,		moz_new_window_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_VISIBILITY,		moz_visibility_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_DESTROY,		moz_destroy_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_CONTEXT,		moz_context_cb, NULL);
    PtAddCallback(info->web, Pt_CB_MOZ_PRINT_STATUS,	moz_print_status_cb, NULL);
	
  	window_count++;

  	return (info->window);
}
/////////////////////////////////////////////////////////////////////////////


int
main(int argc, char **argv)
{
	unsigned window_flags = ~0;
	PtWidget_t *win;
	struct window_info *i;

	PtInit(NULL);

	win = create_browser_window(window_flags);
	PtGetResource(win, Pt_ARG_POINTER, &i, 0);

	PtRealizeWidget(i->window);
	PtSetResource(i->web, Pt_ARG_MOZ_GET_URL, "www.qnx.com", 0);

  	PtMainLoop();
}
