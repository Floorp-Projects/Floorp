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
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nsIWebBrowser.h"
#include "nsCWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "MozEmbedChrome.h"
#include "nsIComponentManager.h"
#include "nsIWebNavigation.h"
#include "nsString.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "prlog.h"

#include <Pt.h>
#include "PtMozilla.h"
#include <string.h>

PtWidget_t *window, *back, *forward, *stop, *web, *reload;

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

PRLogModuleInfo *PhWidLog;

int
main(int argc, char **argv)
{
	int n = 0;
	PtArg_t args[10];
	PhDim_t	win_dim = {700, 700};
	PhArea_t area = {{0, 0}, {70, 25}};

  if (!PhWidLog)
  {
    PhWidLog =  PR_NewLogModule("PhWidLog");
  }

	PtSetArg(&args[n++], Pt_ARG_TITLE, "PtMozilla Test\n", 0);
	PtSetArg(&args[n++], Pt_ARG_DIM, &win_dim, 0);
  	window = PtAppInit(NULL, NULL, NULL, n, args);

	n = 0;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Back", 0);
  	back = PtCreateWidget(PtButton, window, n, args);
  	PtAddCallback(back, Pt_CB_ACTIVATE, back_cb, NULL);

	n = 0;
	area.pos.x += 71;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Forward", 0);
  	forward = PtCreateWidget(PtButton, window, n, args);
  	PtAddCallback(forward, Pt_CB_ACTIVATE, forward_cb, NULL);

	n = 0;
	area.pos.x += 71;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Reload", 0);
  	reload = PtCreateWidget(PtButton, window, n, args);
  	PtAddCallback(reload, Pt_CB_ACTIVATE, reload_cb, NULL);

	n = 0;
	area.pos.x += 71;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
	PtSetArg(&args[n++], Pt_ARG_TEXT_STRING, "Stop", 0);
  	stop = PtCreateWidget(PtButton, window, n, args);
  	PtAddCallback(stop, Pt_CB_ACTIVATE, stop_cb, NULL);

	n = 0;
	area.pos.x += 71;
	area.size.w = win_dim.w - area.pos.x - 2;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
  	stop = PtCreateWidget(PtText, window, n, args);
  	PtAddCallback(stop, Pt_CB_ACTIVATE, load_url_cb, NULL);

	n = 0;
	area.pos.y += area.size.h + 2;
	area.pos.x = 0;
	area.size.w = win_dim.w;
	area.size.h = win_dim.h - area.pos.y;
	PtSetArg(&args[n++], Pt_ARG_AREA, &area, 0);
  	web = PtCreateWidget(PtMozilla, window, n, args);

  	PtRealizeWidget(window);

  	PtMainLoop();
}
