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
 *	 Brian Edmond <briane@qnx.com>
 */


#ifndef __PT_MOZILLA_H_INCLUDED
#define __PT_MOZILLA_H_INCLUDED

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nsString.h"
#include "nsIWebBrowser.h"
#include "nsCWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "PhMozEmbedChrome.h"
#include "nsIComponentManager.h"
#include "nsIWebNavigation.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FALSE PR_FALSE
#define TRUE PR_TRUE

typedef PtWidget_t PhMozEmbed;

/*
 * PtMozilla public
 */


/* These are straight out of nsIWebProgressListener.h */

typedef enum
{
  Pt_MOZ_FLAG_START = 1,
  Pt_MOZ_FLAG_REDIRECTING = 2,
  Pt_MOZ_FLAG_TRANSFERRING = 4,
  Pt_MOZ_FLAG_NEGOTIATING = 8,
  Pt_MOZ_FLAG_STOP = 16,
  
  Pt_MOZ_FLAG_IS_REQUEST = 65536,
  Pt_MOZ_FLAG_IS_DOCUMENT = 131072,
  Pt_MOZ_FLAG_IS_NETWORK = 262144,
  Pt_MOZ_FLAG_IS_WINDOW = 524288 
} PhMozEmbedProgressFlags;

/* These are from various networking headers */

typedef enum
{
  /* NS_ERROR_UNKNOWN_HOST */
  Pt_MOZ_STATUS_FAILED_DNS     = 2152398878U,
 /* NS_ERROR_CONNECTION_REFUSED */
  Pt_MOZ_STATUS_FAILED_CONNECT = 2152398861U,
 /* NS_ERROR_NET_TIMEOUT */
  Pt_MOZ_STATUS_FAILED_TIMEOUT = 2152398862U,
 /* NS_BINDING_ABORTED */
  Pt_MOZ_STATUS_FAILED_USERCANCELED = 2152398850U
} PhMozEmbedStatusFlags;

/* These are straight out of nsIWebNavigation.h */

typedef enum 
{
  Pt_MOZ_FLAG_RELOADNORMAL = 0,
  Pt_MOZ_FLAG_RELOADBYPASSCACHE = 1,
  Pt_MOZ_FLAG_RELOADBYPASSPROXY = 2,
  Pt_MOZ_FLAG_RELOADBYPASSPROXYANDCACHE = 3 
} PhMozEmbedReloadFlags;

/* These are straight out of nsIWebBrowserChrome.h */

typedef enum
{
  Pt_MOZ_FLAG_DEFAULTCHROME = 1U,
  Pt_MOZ_FLAG_WINDOWBORDERSON = 2U,
  Pt_MOZ_FLAG_WINDOWCLOSEON = 4U,
  Pt_MOZ_FLAG_WINDOWRESIZEON = 8U,
  Pt_MOZ_FLAG_MENUBARON = 16U,
  Pt_MOZ_FLAG_TOOLBARON = 32U,
  Pt_MOZ_FLAG_LOCATIONBARON = 64U,
  Pt_MOZ_FLAG_STATUSBARON = 128U,
  Pt_MOZ_FLAG_PERSONALTOOLBARON = 256U,
  Pt_MOZ_FLAG_SCROLLBARSON = 512U,
  Pt_MOZ_FLAG_TITLEBARON = 1024U,
  Pt_MOZ_FLAG_EXTRACHROMEON = 2048U,
  Pt_MOZ_FLAG_ALLCHROME = 4094U,
  Pt_MOZ_FLAG_WINDOWRAISED = 33554432U,
  Pt_MOZ_FLAG_WINDOWLOWERED = 67108864U,
  Pt_MOZ_FLAG_CENTERSCREEN = 134217728U,
  Pt_MOZ_FLAG_DEPENDENT = 268435456U,
  Pt_MOZ_FLAG_MODAL = 536870912U,
  Pt_MOZ_FLAG_OPENASDIALOG = 1073741824U,
  Pt_MOZ_FLAG_OPENASCHROME = 2147483648U 
} PhMozEmbedChromeFlags;

extern PtWidgetClassRef_t *PtMozilla;

/* Resources */

#define Pt_ARG_MOZ_GET_URL				Pt_RESOURCE( Pt_USER(10),  0 )
#define Pt_ARG_MOZ_NAVIGATE_PAGE		Pt_RESOURCE( Pt_USER(10),  1 )
#define Pt_ARG_MOZ_RELOAD				Pt_RESOURCE( Pt_USER(10),  2 )
#define Pt_ARG_MOZ_STOP					Pt_RESOURCE( Pt_USER(10),  3 )

#define Pt_CB_MOZ_STATUS				Pt_RESOURCE( Pt_USER(10),  4 )
#define Pt_CB_MOZ_START					Pt_RESOURCE( Pt_USER(10),  5 )
#define Pt_CB_MOZ_COMPLETE				Pt_RESOURCE( Pt_USER(10),  6 )
#define Pt_CB_MOZ_PROGRESS				Pt_RESOURCE( Pt_USER(10),  7 )
#define Pt_CB_MOZ_URL					Pt_RESOURCE( Pt_USER(10),  8 )
#define Pt_CB_MOZ_EVENT					Pt_RESOURCE( Pt_USER(10),  9 )
#define Pt_CB_MOZ_NET_STATE             Pt_RESOURCE( Pt_USER(10),  10 )
#define Pt_CB_MOZ_NEW_WINDOW            Pt_RESOURCE( Pt_USER(10),  11 )
#define Pt_CB_MOZ_RESIZE                Pt_RESOURCE( Pt_USER(10),  12 )
#define Pt_CB_MOZ_DESTROY               Pt_RESOURCE( Pt_USER(10),  13 )
#define Pt_CB_MOZ_VISIBILITY            Pt_RESOURCE( Pt_USER(10),  14 )
#define Pt_CB_MOZ_OPEN            		Pt_RESOURCE( Pt_USER(10),  15 )

#define MAX_URL_LENGTH		1024

#define WWW_DIRECTION_FWD	0x1
#define WWW_DIRECTION_BACK	0x2

// progress callback, can be itemized or full
#define Pt_MOZ_PROGRESS		1
#define Pt_MOZ_PROGRESS_ALL	2
typedef struct mozilla_progress_t
{
	int 	type;
	int32 	cur;
	int32 	max;
} PtMozillaProgressCb_t;

// url change callback
typedef struct mozilla_url_t
{
	char *url;
} PtMozillaUrlCb_t;

// status callback for Java Script, link messages, titles
#define Pt_MOZ_STATUS_LINK		1
#define Pt_MOZ_STATUS_JS		2
#define Pt_MOZ_STATUS_TITLE		3
typedef struct mozilla_status_t
{
	int		type;
	char 	*message;
} PtMozillaStatusCb_t;


typedef struct mozilla_event_t
{
	int		type;
} PtMozillaEventCb_t;

typedef struct mozilla_net_state_t
{
        int 			flags;
        unsigned int 	status;
        char 			*request;
} PtMozillaNetStateCb_t;

// new window callback
// see PhMozEmbedChromeFlags for window_flags
typedef struct mozilla_new_window_t
{
	PtWidget_t 		*widget;
	unsigned int	window_flags;
} PtMozillaNewWindowCb_t;

typedef struct mozilla_resize_t
{
        PhDim_t dim;
} PtMozillaResizeCb_t;

class MozEmbedPrivate
{
public:
  nsCOMPtr<nsIWebBrowser>     	webBrowser;
  nsCOMPtr<nsIWebNavigation>  	navigation;
  nsCOMPtr<nsIPhEmbed>       	embed;
  nsCString           			mInitialURL;
};

class PhMozEmbedPrivate;
class PhMozEmbedChrome;

typedef struct Pt_mozilla_client_widget 
{
	PtContainerWidget_t	container;
	char				url[MAX_URL_LENGTH];
	PhMozEmbedPrivate 	*embed_private;
	PhMozEmbedChrome 		*chrome;
	int navigate_flags;
	PtCallbackList_t 	*title_cb;
	PtCallbackList_t 	*net_state_cb;
	PtCallbackList_t 	*status_cb;
	PtCallbackList_t 	*progress_cb;
	PtCallbackList_t 	*start_cb;
	PtCallbackList_t 	*complete_cb;
	PtCallbackList_t 	*url_cb;
	PtCallbackList_t 	*event_cb;
	PtCallbackList_t 	*resize_cb;
	PtCallbackList_t 	*new_window_cb;
	PtCallbackList_t 	*destroy_cb;
	PtCallbackList_t 	*visibility_cb;
	PtCallbackList_t 	*open_cb;
} PtMozillaWidget_t;

/* Widget union */
typedef union Pt_mozilla_union 
{
	PtWidget_t				core;
	PtBasicWidget_t			basic;
	PtContainerWidget_t		cntnr;
	PtMozillaWidget_t		moz;
} PtMozillaUnion_t;

#ifdef __cplusplus
};
#endif

#endif
