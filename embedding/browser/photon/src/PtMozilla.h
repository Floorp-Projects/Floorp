/*
 *	PtWebClient.h
 *		Header file for the PtWebClient widget class
 *
 *  Copyright by QNX Software Systems Limited 1996. All rights reserved.
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
#include "MozEmbedChrome.h"
#include "nsIComponentManager.h"
#include "nsIWebNavigation.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PtMozilla public
 */

extern PtWidgetClassRef_t *PtMozilla;

enum { MOZ_EMBED_FLAG_START = 1,
       MOZ_EMBED_FLAG_REDIRECTING = 2,
       MOZ_EMBED_FLAG_TRANSFERRING = 4,
       MOZ_EMBED_FLAG_NEGOTIATING = 8,
       MOZ_EMBED_FLAG_STOP = 16,

       MOZ_EMBED_FLAG_IS_REQUEST = 65536,
       MOZ_EMBED_FLAG_IS_DOCUMENT = 131072,
       MOZ_EMBED_FLAG_IS_NETWORK = 262144,
       MOZ_EMBED_FLAG_IS_WINDOW = 524288 };

/* Resources */

#define Pt_ARG_MOZ_GET_URL				Pt_RESOURCE( Pt_USER(10),  0 )
#define Pt_ARG_MOZ_NAVIGATE_PAGE		Pt_RESOURCE( Pt_USER(10),  1 )
#define Pt_ARG_MOZ_RELOAD				Pt_RESOURCE( Pt_USER(10),  2 )
#define Pt_ARG_MOZ_STOP					Pt_RESOURCE( Pt_USER(10),  3 )
#define Pt_CB_MOZ_STATUS				Pt_RESOURCE( Pt_USER(10),  4 )
#define Pt_CB_MOZ_TITLE					Pt_RESOURCE( Pt_USER(10),  5 )
#define Pt_CB_MOZ_PROGRESS				Pt_RESOURCE( Pt_USER(10),  6 )
#define Pt_CB_MOZ_START					Pt_RESOURCE( Pt_USER(10),  7 )
#define Pt_CB_MOZ_COMPLETE				Pt_RESOURCE( Pt_USER(10),  8 )
#define Pt_CB_MOZ_URL					Pt_RESOURCE( Pt_USER(10),  9 )

#define MAX_URL_LENGTH		1024

#define WWW_DIRECTION_FWD	1
#define WWW_DIRECTION_BACK	2

typedef struct mozilla_progress_t
{
	int32 cur;
	int32 max;
} PtMozillaProgressCb_t;

typedef struct mozilla_title_t
{
	char *title;
} PtMozillaTitleCb_t;

typedef struct mozilla_url_t
{
	char *url;
} PtMozillaUrlCb_t;

typedef struct mozilla_status_t
{
	char *message;
} PtMozillaStatusCb_t;

class MozEmbedPrivate
{
public:
  nsCOMPtr<nsIWebBrowser>     	webBrowser;
  nsCOMPtr<nsIWebNavigation>  	navigation;
  nsCOMPtr<nsIPhEmbed>       	embed;
  nsCString           			mInitialURL;
};

typedef struct Pt_mozilla_client_widget 
{
	PtContainerWidget_t	container;
	char				url[MAX_URL_LENGTH];
	MozEmbedPrivate 	*embed_private;
	MozEmbedChrome 		*chrome;
	PtCallbackList_t 	*title_cb;
	PtCallbackList_t 	*status_cb;
	PtCallbackList_t 	*progress_cb;
	PtCallbackList_t 	*start_cb;
	PtCallbackList_t 	*complete_cb;
	PtCallbackList_t 	*url_cb;
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
