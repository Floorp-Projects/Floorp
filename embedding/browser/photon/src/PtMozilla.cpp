/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Ramiro Estrugo <ramiro@eazel.com>
 *   Brian Edmond <briane@qnx.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <stdlib.h>

#include "nsCWebBrowser.h"
#include "nsILocalFile.h"
#include "nsISelectionController.h"
#include "nsEmbedAPI.h"

#include "nsWidgetsCID.h"
#include "nsIAppShell.h"

#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"

// for profiles
#include <nsProfileDirServiceProvider.h>

#include "nsIDocumentLoaderFactory.h"
#include "nsILoadGroup.h"
#include "nsIHistoryEntry.h"

#include "nsIWebBrowserPrint.h"
#include "nsIPrintOptions.h"

#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMWindow.h"
#include "nsNetUtil.h"
#include "nsIFocusController.h"
#include <nsIWebBrowserFind.h>

#include "nsReadableUtils.h"

#include "nsIPresShell.h"

#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIURIFixup.h"
#include "nsPIDOMWindow.h"

#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"
#include "nsUnknownContentTypeHandler.h"

#include "EmbedPrivate.h"
#include "EmbedWindow.h"
#include "EmbedDownload.h"
#include "HeaderSniffer.h"
#include "PromptService.h"
#include "PtMozilla.h"

//#include "nsUnknownContentTypeHandler.h"

#ifdef _BUILD_STATIC_BIN
#include "nsStaticComponent.h"
nsresult PR_CALLBACK
ph_getModuleInfo(nsStaticModuleInfo **info, PRUint32 *count);
#endif

// Macro for converting from nscolor to PtColor_t
// Photon RGB values are stored as 00 RR GG BB
// nscolor RGB values are 00 BB GG RR
#define NS_TO_PH_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)
#define PH_TO_NS_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

// Class IDs
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#define NS_PROMPTSERVICE_CID \
 {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}
static NS_DEFINE_CID(kPromptServiceCID, NS_PROMPTSERVICE_CID);

static void mozilla_set_pref( PtWidget_t *widget, char *option, char *value );
static void mozilla_get_pref( PtWidget_t *widget, char *option, char *value );
static void fix_translation_string( char *string );

PtWidgetClass_t *PtCreateMozillaClass( void );
#ifndef _PHSLIB
	PtWidgetClassRef_t __PtMozilla = { NULL, PtCreateMozillaClass };
	PtWidgetClassRef_t *PtMozilla = &__PtMozilla; 
#endif

/* globals */
char *g_Print_Left_Header_String, *g_Print_Right_Header_String, *g_Print_Left_Footer_String, *g_Print_Right_Footer_String;

void 
MozSetPreference(PtWidget_t *widget, int type, char *pref, void *data)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	nsIPref *prefs = moz->EmbedRef->GetPrefs();

	switch (type)
	{
		case Pt_MOZ_PREF_CHAR:
			prefs->SetCharPref(pref, (char *)data);
			break;
		case Pt_MOZ_PREF_BOOL:
			prefs->SetBoolPref(pref, (int)data);
			break;
		case Pt_MOZ_PREF_INT:
			prefs->SetIntPref(pref, (int)data);
			break;
		case Pt_MOZ_PREF_COLOR:
// not supported yet
//			prefs->SetColorPrefDWord(pref, (uint32) data);
			break;
	}
}

static void 
MozLoadURL(PtMozillaWidget_t *moz, char *url)
{
  	// If the widget isn't realized, just return.
	if (!(PtWidgetFlags((PtWidget_t *)moz) & Pt_REALIZED))
		return;

	moz->EmbedRef->SetURI(url);
	moz->EmbedRef->LoadCurrentURI();
}

/* watch for an Ph_EV_INFO event in order to detect an Ph_OFFSCREEN_INVALID */
static int EvInfo( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
	if( cbinfo->event && cbinfo->event->type == Ph_EV_INFO && cbinfo->event->subtype == Ph_OFFSCREEN_INVALID ) {
		PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) widget;
		nsIPref *pref = moz->EmbedRef->GetPrefs();
		PRBool displayInternalChange = PR_FALSE;
		pref->GetBoolPref("browser.display.internaluse.graphics_changed", &displayInternalChange);
		pref->SetBoolPref("browser.display.internaluse.graphics_changed", !displayInternalChange);
		}
	return Pt_CONTINUE;
}

const char* const kPersistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";

void MozSaveTarget( char *url, PtMozillaWidget_t *moz )
{
	nsresult rv;
	nsCOMPtr<nsIWebBrowserPersist> webPersist(do_CreateInstance(kPersistContractID, &rv));
	if( !webPersist ) return;

	nsCOMPtr<nsIURI> uri;
	NS_NewURI( getter_AddRefs(uri), url );

	/* create a temporary file */
	char tmp_path[1024];
	tmpnam( tmp_path );
	nsCOMPtr<nsILocalFile> tmpFile;
	NS_NewNativeLocalFile(nsDependentCString( tmp_path ), PR_TRUE, getter_AddRefs(tmpFile));

	/* create a download object, use to sniff the headers for a location indication */
	HeaderSniffer *sniffer = new HeaderSniffer( webPersist, moz, uri, tmpFile );

	webPersist->SetProgressListener( sniffer );
	webPersist->SaveURI( uri, nsnull, nsnull, nsnull, nsnull, tmpFile );
}

// defaults function, called on creation of a widget
static void 
mozilla_defaults( PtWidget_t *widget )
{
	PtMozillaWidget_t 	*moz = (PtMozillaWidget_t *) widget;
	PtBasicWidget_t 	*basic = (PtBasicWidget_t *) widget;
	PtContainerWidget_t *cntr = (PtContainerWidget_t*) widget;

	moz->EmbedRef = new EmbedPrivate();
	moz->EmbedRef->Init(widget);
	moz->EmbedRef->Setup();

	moz->disable_new_windows = 0;
	moz->disable_exception_dlg = 0;
	moz->text_zoom = 100;
	moz->actual_text_zoom = 100;
	moz->toActivate = 0;

	// widget related
	basic->flags = Pt_ALL_OUTLINES | Pt_ALL_BEVELS | Pt_FLAT_FILL;
	widget->resize_flags &= ~Pt_RESIZE_XY_BITS; // fixed size.
	widget->anchor_flags = Pt_TOP_ANCHORED_TOP | Pt_LEFT_ANCHORED_LEFT | \
			Pt_BOTTOM_ANCHORED_TOP | Pt_RIGHT_ANCHORED_LEFT | Pt_ANCHORS_INVALID;

	cntr->flags |= Pt_CHILD_GETTING_FOCUS;

	PtAddEventHandler( widget, Ph_EV_INFO, EvInfo, NULL );
}

// widget destroy function
static void 
mozilla_destroy( PtWidget_t *widget ) 
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	if (moz->EmbedRef)
	{
		moz->EmbedRef->Destroy();
		delete moz->EmbedRef;
	}
}

static int child_getting_focus( PtWidget_t *widget, PtWidget_t *child, PhEvent_t *ev ) {
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	nsCOMPtr<nsPIDOMWindow> piWin;

	moz->EmbedRef->GetPIDOMWindow( getter_AddRefs( piWin ) );
	if( !piWin ) return Pt_CONTINUE;

	nsIFocusController *focusController = piWin->GetRootFocusController();
	if( focusController )
		focusController->SetActive( PR_TRUE );

	if( moz->toActivate ) {
		moz->toActivate = 0;
		piWin->Activate();
		}

	return Pt_CONTINUE;
	}

static int child_losing_focus( PtWidget_t *widget, PtWidget_t *child, PhEvent_t *ev ) {
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	nsCOMPtr<nsPIDOMWindow> piWin;

	moz->EmbedRef->GetPIDOMWindow( getter_AddRefs( piWin ) );
	if( !piWin ) return Pt_CONTINUE;

	nsIFocusController *focusController = piWin->GetRootFocusController();
	if( focusController )
		focusController->SetActive( PR_FALSE );

	piWin->Deactivate();
	moz->toActivate = 1;

	return Pt_CONTINUE;
	}

static void 
mozilla_extent(PtWidget_t *widget)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	PtSuperClassExtent(PtContainer, widget);

	moz->EmbedRef->Position(widget->area.pos.x, widget->area.pos.y);
	moz->EmbedRef->Size(widget->area.size.w, widget->area.size.h);
}

// set resources function
static void 
mozilla_modify( PtWidget_t *widget, PtArg_t const *argt, PtResourceRec_t const *mod )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;
	nsIPref *pref = moz->EmbedRef->GetPrefs();

	switch( argt->type )  
	{
		case Pt_ARG_MOZ_GET_URL:
			MozLoadURL(moz, (char *)(argt->value));
			break;

		case Pt_ARG_MOZ_NAVIGATE_PAGE:
			if (moz->EmbedRef)
			{
				if (argt->value == Pt_WEB_DIRECTION_FWD)
					moz->EmbedRef->Forward();
				else if (argt->value == Pt_WEB_DIRECTION_BACK)
					moz->EmbedRef->Back();
				else 
				{
					PhDim_t dim;

					PtWidgetDim(widget, &dim);
					dim.w = (argt->value * dim.w)/100;
					dim.h = (argt->value * dim.h)/100;

					if (argt->value == Pt_WEB_DIRECTION_UP)
						moz->EmbedRef->ScrollUp(dim.h);
					else if (argt->value ==	Pt_WEB_DIRECTION_DOWN)
						moz->EmbedRef->ScrollDown(dim.h);
					else if (argt->value ==	Pt_WEB_DIRECTION_LEFT)
						moz->EmbedRef->ScrollLeft(dim.w);
					else if (argt->value == Pt_WEB_DIRECTION_RIGHT)
						moz->EmbedRef->ScrollRight(dim.w);
				}
			}
			break;

		case Pt_ARG_MOZ_STOP:
			if (moz->EmbedRef)
				moz->EmbedRef->Stop();
			break;

		case Pt_ARG_MOZ_RELOAD:
			if (moz->EmbedRef)
				moz->EmbedRef->Reload(0);
			break;

		case Pt_ARG_MOZ_PRINT: 
			{
			PpPrintContext_t *pc = ( PpPrintContext_t * ) argt->value;
			moz->EmbedRef->Print(pc);
			}
		  break;

		case Pt_ARG_MOZ_OPTION:
			mozilla_set_pref(widget, (char*)argt->len, (char*)argt->value);
			break;

		case Pt_ARG_MOZ_ENCODING: {
			char translation[1024];
			strcpy( translation, (char*)argt->value );
			fix_translation_string( translation );
 			pref->SetUnicharPref( "intl.charset.default", NS_ConvertASCIItoUTF16( translation ).get());
			pref->SavePrefFile( nsnull );
			}
			break;

		case Pt_ARG_MOZ_COMMAND: {
			PtWebClient2Command_t *wdata = ( PtWebClient2Command_t * ) argt->len;
			switch ((int)(argt->value)) 
			{
				case Pt_MOZ_COMMAND_CUT: {
					moz->EmbedRef->Cut(wdata?wdata->ClipboardInfo.input_group:1);
					}
					break;
				case Pt_MOZ_COMMAND_COPY: {
					moz->EmbedRef->Copy(wdata?wdata->ClipboardInfo.input_group:1);
					}
					break;
				case Pt_MOZ_COMMAND_PASTE: {
					moz->EmbedRef->Paste(wdata?wdata->ClipboardInfo.input_group:1);
					}
					break;
				case Pt_MOZ_COMMAND_SELECTALL: {
					moz->EmbedRef->SelectAll();
					}
					break;
				case Pt_MOZ_COMMAND_CLEAR: {
					moz->EmbedRef->Clear();
					}
					break;

				case Pt_MOZ_COMMAND_FIND: {
					nsCOMPtr<nsIWebBrowserFind> finder( do_GetInterface( moz->EmbedRef->mWindow->mWebBrowser ) );
					finder->SetSearchString( NS_ConvertASCIItoUTF16(wdata->FindInfo.string).get() );
					finder->SetMatchCase( wdata->FindInfo.flags & Pt_WEB_FIND_MATCH_CASE );
					finder->SetFindBackwards( wdata->FindInfo.flags & Pt_WEB_FIND_GO_BACKWARDS );
					finder->SetWrapFind( wdata->FindInfo.flags & Pt_WEB_FIND_START_AT_TOP );
					finder->SetEntireWord( wdata->FindInfo.flags & Pt_WEB_FIND_MATCH_WHOLE_WORDS );

					PRBool didFind;
					finder->FindNext( &didFind );
					break;
					}

				case Pt_MOZ_COMMAND_SAVEAS: {
					char *dirname = ( char * ) calloc( 1, strlen( wdata->SaveasInfo.filename + 7 ) );
					if( dirname ) {
						sprintf( dirname, "%s_files", wdata->SaveasInfo.filename );
						moz->EmbedRef->SaveAs( wdata->SaveasInfo.filename, dirname );
						free( dirname );
						}
					break;
					}
				}
			}
			break;

		case Pt_ARG_MOZ_WEB_DATA_URL:
#if 0
//				moz->MyBrowser->WebBrowserContainer->OpenStream( moz->MyBrowser->WebBrowser, "file://", "text/html" );
//				strcpy( moz->url, (char*)argt->value );
#endif
				break;

		case Pt_ARG_MOZ_UNKNOWN_RESP: {
			PtWebClient2UnknownData_t *unknown = ( PtWebClient2UnknownData_t * ) argt->value;
			if( unknown->response == Pt_WEB_RESPONSE_CANCEL ) {
				EmbedDownload *d = FindDownload( moz, unknown->download_ticket );
				if( d ) {
					if( d->mLauncher ) d->mLauncher->Cancel(NS_BINDING_ABORTED); /* this will also call the EmbedDownload destructor */
					else if( d->mPersist ) d->mPersist->CancelSave(); /* this will also call the EmbedDownload destructor */
					else delete d; /* just in case neither d->mLauncher or d->mPersist was set */
					}
				}
			}
			break;

		case Pt_ARG_MOZ_DOWNLOAD: 
			{
				MozSaveTarget( (char*)argt->value, moz );
			}
			break;

		case Pt_ARG_MOZ_WEB_DATA: {
#if 0
				WWWRequest *req = ( WWWRequest * ) argt->value;
				char *hdata;
				int len;

				if( strcmp( moz->url, req->DataHdr.url ) ) break;

				hdata = (char*) argt->len;
				len = req->DataHdr.length;

				switch( req->DataHdr.type ) {
					case Pt_WEB_DATA_HEADER:
						if( !moz->MyBrowser->WebBrowserContainer->IsStreaming( ) ) break;

						/* request the document body */
						if( moz->web_data_req_cb ) {
							PtCallbackInfo_t  cbinfo;
							PtMozillaDataRequestCb_t cb;

							memset( &cbinfo, 0, sizeof( cbinfo ) );
							cbinfo.reason = Pt_CB_MOZ_WEB_DATA_REQ;
							cbinfo.cbdata = &cb;
							cb.type = Pt_WEB_DATA_BODY;
							cb.length = 32768;
							cb.url = moz->url;
							PtInvokeCallbackList( moz->web_data_req_cb, (PtWidget_t *)moz, &cbinfo);
							}
						break;
					case Pt_WEB_DATA_BODY: {
						if( !moz->MyBrowser->WebBrowserContainer->IsStreaming( ) ) break;
						if( len )
							moz->MyBrowser->WebBrowserContainer->AppendToStream( hdata, len );
						else moz->MyBrowser->WebBrowserContainer->CloseStream( );

						/* request the next piece of the body */
						PtCallbackInfo_t  cbinfo;
						PtMozillaDataRequestCb_t cb;

						memset( &cbinfo, 0, sizeof( cbinfo ) );
						cbinfo.reason = Pt_CB_MOZ_WEB_DATA_REQ;
						cbinfo.cbdata = &cb;
						cb.type = len ? Pt_WEB_DATA_BODY : Pt_WEB_DATA_CLOSE;
						cb.length = 32768;
						cb.url = moz->url;
						PtInvokeCallbackList( moz->web_data_req_cb, (PtWidget_t *)moz, &cbinfo);
						}
						break;
					case Pt_WEB_DATA_CLOSE:
						if( !moz->MyBrowser->WebBrowserContainer->IsStreaming( ) ) break;
						moz->MyBrowser->WebBrowserContainer->CloseStream( );
						break;
					}
#endif
				}
			break;
		}

	return;
}


// get resources function
static int 
mozilla_get_info( PtWidget_t *widget, PtArg_t *argt, PtResourceRec_t const *mod )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	nsIPref *pref = moz->EmbedRef->GetPrefs();

	switch (argt->type)
	{
		case Pt_ARG_MOZ_NAVIGATE_PAGE:
			moz->navigate_flags = 0;
			if (moz->EmbedRef->CanGoBack())
				moz->navigate_flags |= ( 1 << Pt_WEB_DIRECTION_BACK );
			if (moz->EmbedRef->CanGoForward())
				moz->navigate_flags |= ( 1 << Pt_WEB_DIRECTION_FWD );
			*((int **)argt->value) = &moz->navigate_flags;
			break;

		case Pt_ARG_MOZ_OPTION:
			mozilla_get_pref( widget, (char*)argt->len, (char*) argt->value );
			break;

		case Pt_ARG_MOZ_GET_CONTEXT: {
			if( argt->len & Pt_MOZ_CONTEXT_LINK )
				*(char**) argt->value = moz->rightClickUrl_link;
			else if( argt->len & Pt_MOZ_CONTEXT_IMAGE )
				*(char**) argt->value = moz->rightClickUrl_image;
			else *(char**) argt->value = moz->rightClickUrl_link;
			}
			break;

		case Pt_ARG_MOZ_ENCODING: 
			{
			PRUnichar *charset = nsnull;
			static char encoding[256];
			pref->GetLocalizedUnicharPref( "intl.charset.default", &charset );
			strcpy( encoding, NS_ConvertUTF16toUTF8(charset).get() );
			*(char**)argt->value = encoding;
			}
			break;

		case Pt_ARG_MOZ_GET_HISTORY: 
			{
				PtWebClientHistory_t *hist_info = (PtWebClientHistory_t *)argt->len;
				PtWebClientHistoryData_t *HistoryReplyBuf = (PtWebClientHistoryData_t *) argt->value;
				int i, j;
				PRInt32 total;
				moz->EmbedRef->mSessionHistory->GetCount( &total );

				for( i=total-2, j=0; i>=0 && j<hist_info->num; i--, j++) 
				{
					nsIHistoryEntry *entry;
					moz->EmbedRef->mSessionHistory->GetEntryAtIndex( i, PR_FALSE, &entry );
				
					PRUnichar *title;
					nsIURI *url;
					entry->GetTitle( &title );
					entry->GetURI( &url );

					nsString stitle( title );
					HistoryReplyBuf[j].title = strdup( ToNewCString(stitle) );

					nsCAutoString specString;
					url->GetSpec(specString);
					HistoryReplyBuf[j].url = strdup( (char *) specString.get() );
				}
			}
			break;
	}

	return (Pt_CONTINUE);
}


static void 
mozilla_set_pref( PtWidget_t *widget, char *option, char *value ) 
{
	PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) widget;
	nsIPref *pref = moz->EmbedRef->GetPrefs();
	char buffer[1024];

	mozilla_get_pref( widget, option, buffer );
	if( buffer[0] && !strcmp( value, buffer ) )
		return; /* the option is already set */

/* HTML Options */
	if( !strcmp( option, "A:visited color" ) ) {
		pref->SetUnicharPref( "browser.visited_color", NS_ConvertASCIItoUTF16(value).get() );
		}
	else if( !strcmp( option, "A:link color" ) ) {
		pref->SetUnicharPref( "browser.anchor_color", NS_ConvertASCIItoUTF16(value).get() );
		}

/* the mozserver already has A:link color == browser.anchor_color for this */
//	else if( !strcmp( option, "A:active color" ) ) {
//		pref->SetUnicharPref( "browser.anchor_color", NS_ConvertASCIItoUTF16(value).get() );
//		}

	else if( !strcmp( option, "BODY color" ) ) {
		pref->SetUnicharPref( "browser.display.foreground_color", NS_ConvertASCIItoUTF16(value).get() );
		}
	else if( !strcmp( option, "BODY background" ) ) {
		pref->SetUnicharPref( "browser.display.background_color", NS_ConvertASCIItoUTF16(value).get() );
		}
	else if( !strcmp( option, "bIgnoreDocumentAttributes" ) )
		pref->SetBoolPref( "browser.display.use_document_colors", stricmp( value, "TRUE" ) ? PR_FALSE : PR_TRUE );
	else if( !strcmp( option, "bUnderlineLinks" ) ) {
		pref->SetBoolPref( "browser.underline_anchors", !stricmp( value, "TRUE" ) ? PR_TRUE : PR_FALSE );
		}
	else if( !strcmp( option, "iUserTextSize" ) ) {
		moz->text_zoom = atoi( value );
		nsCOMPtr<nsIDOMWindow> domWindow;
		moz->EmbedRef->mWindow->mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
		if(domWindow) {
			domWindow->SetTextZoom( moz->text_zoom/100. );
			float vv;
			domWindow->GetTextZoom( &vv );
			moz->actual_text_zoom = (int) ( vv * 100 );
			}
		}
	else if( !strcmp( option, "BODY font-family" ) ) {
		/* set the current font */
		char *font_default = NULL;
		char preference[256];

		pref->CopyCharPref( "font.default.x-western", &font_default );
		if( !font_default ) font_default = "serif";

		sprintf( preference, "font.name.%s.x-western", font_default );
		pref->SetCharPref( preference, value );
		}
  else if( !strcmp( option, "PRE font-family" ) ) {
    pref->SetCharPref( "font.name.monospace.x-western", value );
    }
	else if( !strcmp( option, "H* font-family" ) ) {
		/* do not set these - use the BODY font-family instead */
		}

/* SOCKS options */
	else if( !strcmp( option, "socks_server" ) )	; /* not used */
	else if( !strcmp( option, "socks_port" ) )		; /* not used */
	else if( !strcmp( option, "socks_user" ) ) 		; /* not used */
	else if( !strcmp( option, "socks_app" ) ) 		; /* not used */

/* HTTP options */

	else if( !strcmp( option, "enable_proxy" ) ) {
		if( value && !stricmp( value, "yes" ) )
			pref->SetIntPref( "network.proxy.type", 1 );
			else pref->SetIntPref( "network.proxy.type", 0 );
		}
	else if( !strcmp( option, "http_proxy_host" ) ) {
		pref->SetCharPref( "network.proxy.http", value );
		}
	else if( !strcmp( option, "http_proxy_port" ) ) {
		pref->SetIntPref( "network.proxy.http_port", atoi(value) );
		}
	else if( !strcmp( option, "proxy_overrides" ) ) {
		pref->SetCharPref( "network.proxy.no_proxies_on", value );
		}
	else if( !strcmp( option, "https_proxy_host" ) ) {
		pref->SetCharPref( "network.proxy.ssl", value );
		}
	else if( !strcmp( option, "https_proxy_port" ) ) {
		pref->SetIntPref( "network.proxy.ssl_port", atoi(value) );
		}


/* FTP options */
	else if( !strcmp( option, "ftp_proxy_host" ) )
		pref->SetCharPref( "network.proxy.ftp", value );
	else if( !strcmp( option, "ftp_proxy_port" ) )
		pref->SetIntPref( "network.proxy.ftp_port", atoi(value) );
	else if( !strcmp( option, "email_address" ) )		; /* not used */

/* Gopher options */
	else if( !strcmp( option, "gopher_proxy_host" ) )
		pref->SetCharPref( "network.proxy.gopher", value );
	else if( !strcmp( option, "gopher_proxy_port" ) )
		pref->SetIntPref( "network.proxy.gopher_port", atoi(value) );

/* TCP/IP options */
	else if( !strcmp( option, "max_connections" ) )
		pref->SetIntPref( "network.http.max-connections", atoi( value ) );

/* Disk-cache options */
	else if( !strcmp( option, "main_cache_kb_size" ) )
		pref->SetIntPref( "browser.cache.disk.capacity", atoi( value ) );
	else if( !strcmp( option, "enable_disk_cache" ) )
		pref->SetBoolPref( "browser.cache.disk.enable", !stricmp( value, "TRUE" ) ? PR_TRUE : PR_FALSE );
	else if( !strcmp( option, "dcache_verify_policy" ) ) {
		int n = atoi( value ), moz_value=3;
		if( n == 0 ) moz_value = 2; /* never */
		else if( n == 1 ) moz_value = 0; /* once */
		else if( n == 2 ) moz_value = 1; /* always */

// mozilla wants: 0 = once-per-session, 1 = each-time, 2 = never, 3 = when-appropriate/automatically
		pref->SetIntPref( "browser.cache.check_doc_frequency", moz_value );
		}
	else if( !strcmp( option, "main_cache_dir" ) ) 		; /* not used */
	else if( !strcmp( option, "main_index_file" ) ) 		; /* not used */
	else if( !strcmp( option, "clear_main_cache_on_exit" ) ) 		; /* not used */
	else if( !strcmp( option, "keep_index_file_updated" ) ) 		; /* not used */

/* memory cache options */
  else if( !strcmp( option, "memory_cache_kb_size" ) || !strcmp( option, "image_cache_size_KB" ) ) {
    int kb = atoi( value );
    if( kb <= 0 ) kb = 100; /* have a minimum threshold */
    pref->SetIntPref( "browser.cache.memory.capacity", kb );
    }

/* Miscellaneous options */
	else if( !strcmp( option, "History_Expire" ) )
		pref->SetIntPref( "browser.history_expire_days", atoi( value ) );
	else if( !strcmp( option, "Page_History_Length" ) )
		pref->SetIntPref( "browser.sessionhistory.max_entries", atoi( value ) );



/* HTML Options */
	else if( !strcmp( option, "bAutoLoadImages" ) )			; /* not used */
	else if( !strcmp( option, "iReformatHandling" ) )		; /* not used */
	else if( !strcmp( option, "BODY margin-top" ) )		; /* not used */
	else if( !strcmp( option, "BODY margin-right" ) )		; /* not used */
	else if( !strcmp( option, "iScrollbarSize" ) )		; /* not used */
	else if( !strcmp( option, "top_focus_color" ) )		; /* not used */
	else if( !strcmp( option, "bot_focus_color" ) )		; /* not used */
	else if( !strcmp( option, "top_border_color" ) )		; /* not used */
	else if( !strcmp( option, "bot_border_color" ) )		; /* not used */
	else if( !strcmp( option, "bview_source" ) )		; /* not used */
	else if( !strcmp( option, "underline_width" ) )		; /* not used */
	else if( !strcmp( option, "bkey_mode" ) )		; /* not used */
	else if( !strcmp( option, "mono_form_font" ) )		; /* not used */
	else if( !strcmp( option, "form_font" ) )		; /* not used */
	else if( !strcmp( option, "frame_spacing" ) )		; /* not used */
	else if( !strcmp( option, "disable_new_windows" ) ) /* disable new windows*/
	{
		if (strcmp(value, "TRUE") == 0)
			moz->disable_new_windows = 1;
		else
			moz->disable_new_windows = 0;
	}
	else if( !strcmp( option, "disable_exception_dlg" ) ) /* disable exception dialogs */ 
	{
		if (strcmp(value, "TRUE") == 0)
			moz->disable_exception_dlg = 1;
		else
			moz->disable_exception_dlg = 0;
	}

	else if( !strcmp( option, "bDisableHighlight" ) )		; /* not used */

/* HTTP cookie options */
	else if( !strcmp( option, "cookiejar_path" ) )		; /* not used */
	else if( !strcmp( option, "cookiejar_name" ) )		; /* not used */
	else if( !strcmp( option, "cookiejar_size" ) )		; /* not used */
	else if( !strcmp( option, "cookiejar_save_always" ) )		; /* not used */

/* Authentication options */
	else if( !strcmp( option, "max_password_guesses" ) )		; /* not used */

/* File options */
	else if( !strcmp( option, "file_display_dir" ) )		; /* not used */

/* Image options */
	else if( !strcmp( option, "bProgressiveImageDisplay" ) ) 		; /* not used */
	else if( !strcmp( option, "quantize_jpegs" ) ) 		; /* not used */
	else if( !strcmp( option, "concurrent_decodes" ) ) 		; /* not used */


/* Print options */
	else if( !strcmp( option, "Print_Header_Font" ) ) 		; /* not used */
	else if( !strcmp( option, "Print_Header_Font_Size" ) ) 		; /* not used */
  else if( !strcmp( option, "Print_Left_Header_String" ) ) {
    if( g_Print_Left_Header_String ) free( g_Print_Left_Header_String );
    g_Print_Left_Header_String = strdup( value );
    }
  else if( !strcmp( option, "Print_Right_Header_String" ) ) {
    if( g_Print_Right_Header_String ) free( g_Print_Right_Header_String );
    g_Print_Right_Header_String = strdup( value );
    }
  else if( !strcmp( option, "Print_Left_Footer_String" ) ) {
    if( g_Print_Left_Footer_String ) free( g_Print_Left_Footer_String );
    g_Print_Left_Footer_String = strdup( value );
    }
  else if( !strcmp( option, "Print_Right_Footer_String" ) ) {
    if( g_Print_Right_Footer_String ) free( g_Print_Right_Footer_String );
    g_Print_Right_Footer_String = strdup( value );
    }


/* Miscellaneous options */
	else if( !strcmp( option, "Global_History_File" ) ) 		; /* not used */
	else if( !strcmp( option, "IBeam_Cursor" ) ) 		; /* not used */
	else if( !strcmp( option, "Image_Cache_Size_KB" ) ) 		; /* not used */
	else if( !strcmp( option, "ImageMap_Cursor" ) ) 		; /* not used */
	else if( !strcmp( option, "ImageMap_Cursor_NoLink" ) ) 		; /* not used */
	else if( !strcmp( option, "Link_Cursor" ) ) 		; /* not used */
	else if( !strcmp( option, "LinkWait_Cursor" ) ) 		; /* not used */
	else if( !strcmp( option, "Normal_Cursor" ) ) 		; /* not used */
	else if( !strcmp( option, "NormalWait_Cursor" ) ) 		; /* not used */
	else if( !strcmp( option, "Page_Cache_Size" ) ) 		; /* not used */
	else if( !strcmp( option, "Safe_Memory_Free" ) ) 		; /* not used */
	else if( !strcmp( option, "Site_History_Length" ) ) 		; /* not used */
	else if( !strcmp( option, "Show_Server_Errors" ) ) 		; /* not used */
	else if( !strcmp( option, "Use_Anti_Alias" ) ) 		; /* not used */
	else if( !strcmp( option, "Use_Explicit_Accept_Headers" ) ) 		; /* not used */
	else if( !strcmp( option, "Visitation_Horizon" ) ) 		; /* not used */

  else if( !strcmp( option, "Print_Frame" ) )
    pref->SetCharPref( "user.print.print_frame", value );
  else if( !strcmp( option, "SetPrintBGColors" ) )
    pref->SetCharPref( "user.print.SetPrintBGColors", value );
  else if( !strcmp( option, "SetPrintBGImages" ) )
    pref->SetCharPref( "user.print.SetPrintBGImages", value );

	pref->SavePrefFile( nsnull );
	}

static void mozilla_get_pref( PtWidget_t *widget, char *option, char *value ) {
	PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) widget;
	nsIPref *pref = moz->EmbedRef->GetPrefs();

	/* HTML Options */
	if( !strcmp( option, "A:link color" ) || !strcmp( option, "A:active color" ) ) {
		nsXPIDLCString colorStr;
		if( pref->CopyCharPref( "browser.anchor_color", getter_Copies(colorStr) ) == NS_OK )
			strcpy( value, colorStr );
		}
	else if( !strcmp( option, "A:visited color" ) ) {
		nsXPIDLCString colorStr;
		if( pref->CopyCharPref( "browser.visited_color", getter_Copies(colorStr) ) == NS_OK )
			strcpy( value, colorStr );
		}
	else if( !strcmp( option, "BODY color" ) ) {
		nsXPIDLCString colorStr;
		if( pref->CopyCharPref( "browser.display.foreground_color", getter_Copies(colorStr) ) == NS_OK )
			strcpy( value, colorStr );
		}
	else if( !strcmp( option, "BODY background" ) ) {
		nsXPIDLCString colorStr;
		if( pref->CopyCharPref( "browser.display.background_color", getter_Copies(colorStr) ) == NS_OK )
			strcpy( value, colorStr );
		}
	else if( !strcmp( option, "bIgnoreDocumentAttributes" ) ) {
		PRBool val;
		pref->GetBoolPref( "browser.display.use_document_colors", &val );
		sprintf( value, "%s", val == PR_TRUE ? "FALSE" : "TRUE" );
		}
	else if( !strcmp( option, "disable_new_windows" ) ) {
		sprintf( value, "%s", (moz->disable_new_windows == 1) ? "TRUE" : "FALSE" );
		}
	else if( !strcmp( option, "disable_exception_dlg" ) ) {
		sprintf( value, "%s", (moz->disable_exception_dlg == 1) ? "TRUE" : "FALSE" );
		}
	else if( !strcmp( option, "bUnderlineLinks" ) ) {
		PRBool val;
		pref->GetBoolPref( "browser.underline_anchors", &val );
		sprintf( value, "%s", val == PR_TRUE ? "TRUE" : "FALSE" );
		}
	else if( !strcmp( option, "iUserTextSize" ) ) {
		sprintf( value, "%d", moz->text_zoom );
		}
	else if( !strcmp( option, "BODY font-family" ) || !strcmp( option, "H* font-family" ) ) {
		/* set the current font */
		char *font_default = NULL, *font;
		char preference[256];

		pref->CopyCharPref( "font.default.x-western", &font_default );
		if( !font_default ) font_default = "serif";

		sprintf( preference, "font.name.%s.x-western", font_default );
		if( pref->CopyCharPref( preference, &font ) == NS_OK )
			strcpy( value, font );
		}
	else if( !strcmp( option, "PRE font-family" ) ) {
		/* set the current font */
		char *font;
		if( pref->CopyCharPref( "font.name.monospace.x-western", &font ) == NS_OK )
			strcpy( value, font );
		}

/* HTTP options */
  else if( !strcmp( option, "http_proxy_host" ) ) {
		char *s;
		if( pref->CopyCharPref( "network.proxy.http", &s ) == NS_OK )
			strcpy( value, s );
		}
  else if( !strcmp( option, "http_proxy_port" ) ) {
		int n;
		pref->GetIntPref( "network.proxy.http_port", &n );
		sprintf( value, "%d", n );
		}
  else if( !strcmp( option, "proxy_overrides" ) ) {
		char *s;
		if( pref->CopyCharPref( "network.proxy.no_proxies_on", &s ) == NS_OK )
			strcpy( value, s );
		}
  else if( !strcmp( option, "https_proxy_host" ) ) {
		char *s;
		if( pref->CopyCharPref( "network.proxy.ssl", &s ) == NS_OK )
			strcpy( value, s );
    }
  else if( !strcmp( option, "https_proxy_port" ) ) {
		int n;
		pref->GetIntPref( "network.proxy.ssl_port", &n );
		sprintf( value, "%d", n );
    }


/* FTP options */
  else if( !strcmp( option, "ftp_proxy_host" ) ) {
		char *s;
		if( pref->CopyCharPref( "network.proxy.ftp", &s ) == NS_OK )
			strcpy( value, s );
		}
  else if( !strcmp( option, "ftp_proxy_port" ) ) {
		int n;
		pref->GetIntPref( "network.proxy.ftp_port", &n );
		sprintf( value, "%d", n );
		}

/* Gopher options */
  else if( !strcmp( option, "gopher_proxy_host" ) ) {
		char *s;
		if( pref->CopyCharPref( "network.proxy.gopher", &s ) == NS_OK )
			strcpy( value, s );
		}
  else if( !strcmp( option, "gopher_proxy_port" ) ) {
		int n;
		pref->GetIntPref( "gopher_proxy_port", &n );
		sprintf( value, "%d", n );
		}

/* TCP/IP options */
  else if( !strcmp( option, "max_connections" ) ) {
		int n;
		pref->GetIntPref( "network.http.max-connections", &n );
		sprintf( value, "%d", n );
		}

/* Disk-cache options */
  else if( !strcmp( option, "main_cache_kb_size" ) ) {
		int n;
		pref->GetIntPref( "browser.cache.disk.capacity", &n );
		sprintf( value, "%d", n );
		}
  else if( !strcmp( option, "enable_disk_cache" ) ) {
		PRBool val;
		pref->GetBoolPref( "browser.cache.disk.enable", &val );
		sprintf( value, "%s", val == PR_TRUE ? "TRUE" : "FALSE" );
		}
  else if( !strcmp( option, "dcache_verify_policy" ) ) {
		int n, voyager_value = 0;
		pref->GetIntPref( "browser.cache.check_doc_frequency", &n );
// 0 = once-per-session, 1 = each-time, 2 = never, 3 = when-appropriate/automatically
		if( n == 0 ) voyager_value = 1; /* voyager: 1 = once per session */
		else if( n == 1 ) voyager_value = 2; /* voyager: 2 = always */
		else if( n == 2 ) voyager_value = 0; /* voyager: 0 = never */
		else voyager_value = 1; /* mapp the when-appropriate/automatically to the once per session */
		sprintf( value, "%d", voyager_value );
    }

/* Miscellaneous options */
  else if( !strcmp( option, "History_Expire" ) ) {
		int n;
		pref->GetIntPref( "browser.history_expire_days", &n );
		sprintf( value, "%d", n );
		}
  else if( !strcmp( option, "Page_History_Length" ) ) {
		int n;
		pref->GetIntPref( "browser.sessionhistory.max_entries", &n );
		sprintf( value, "%d", n );
		}

	else if( !strcmp( option, "clear_main_cache_on_exit" ) ) {
		strcpy( value, "FALSE" ); /* even if not used, match this with the default value */
		}
	else if( !strcmp( option, "quantize_jpegs" ) ) {
		strcpy( value, "FALSE" ); /* even if not used, match this with the default value */
		}
	else if( !strcmp( option, "ServerId" ) ) {
		strcpy( value, "mozserver" );
		}
	else *value = 0;

	}

int sProfileDirCreated;
static int StartupProfile( char *sProfileDir )
{
	// initialize profiles
	if(sProfileDir) 
	{
		nsresult rv;
		nsCOMPtr<nsILocalFile> profileDir;
		PRBool exists = PR_FALSE;
		PRBool isDir = PR_FALSE;
		profileDir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
		rv = profileDir->InitWithNativePath(nsDependentCString(sProfileDir));
		if (NS_FAILED(rv))
		  	return NS_ERROR_FAILURE;
		profileDir->Exists(&exists);
		profileDir->IsDirectory(&isDir);
		// if it exists and it isn't a directory then give up now.
		if (!exists) 
		{
		 	rv = profileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
		 	if( NS_FAILED(rv) )
				return NS_ERROR_FAILURE;
			sProfileDirCreated = 1;
		}
		else if (exists && !isDir)
		 	return NS_ERROR_FAILURE;

		nsCOMPtr<nsProfileDirServiceProvider> locProvider;
		NS_NewProfileDirServiceProvider(PR_TRUE, getter_AddRefs(locProvider));
		if (!locProvider)
		  return NS_ERROR_FAILURE;

		// Directory service holds an strong reference to any
		// provider that is registered with it. Let it hold the
		// only ref. locProvider won't die when we leave this scope.
		rv = locProvider->Register();
		if (NS_FAILED(rv))
		  return rv;
		rv = locProvider->SetProfileDir(profileDir);
		if (NS_FAILED(rv))
		  return rv;
  	}
  
	return NS_OK;
}

// startup the mozilla embedding engine
static int StartupEmbedding()
{
  nsresult rv;

#ifdef _BUILD_STATIC_BIN
  // Initialize XPCOM's module info table
  NSGetStaticModuleInfo = ph_getModuleInfo;
#endif

  rv = NS_InitEmbedding(nsnull, nsnull);
  if( NS_FAILED( rv ) ) return -1;

	char *profile_dir;
	profile_dir = (char *)alloca(strlen(getenv("HOME")) + strlen("/.ph/mozilla") + 1);
	sprintf(profile_dir, "%s/.ph/mozilla", getenv("HOME"));
  rv = StartupProfile( profile_dir );
  if( NS_FAILED( rv ) )
		NS_WARNING("Warning: Failed to start up profiles.\n");
    
  nsCOMPtr<nsIAppShell> appShell;
  appShell = do_CreateInstance(kAppShellCID);
  if( !appShell ) {
	    NS_WARNING("Failed to create appshell in EmbedPrivate::PushStartup!\n");
    	return (-1);
    	}
  nsIAppShell * sAppShell = appShell.get();
  NS_ADDREF(sAppShell);
  sAppShell->Create(0, nsnull);
  sAppShell->Spinup();
	return 0;
}

/* the translation string that is passed by voyager ( Pt_ARG_WEB_ENCODING ) ( taken from /usr/photon/translations/charsets ->mime field ) */
/* is required to be in a different form by the mozilla browser */
static void fix_translation_string( char *string ) {
	/* for instance it has to be windows-1252 not Windows-1252 */
	if( !strncmp( string, "Windows", 7 ) ) *string = 'w';
	}

// PtMozilla class creation function
PtWidgetClass_t *PtCreateMozillaClass( void )
{
	static const PtResourceRec_t resources[] =
	{
		{ Pt_ARG_MOZ_GET_URL,           mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_NAVIGATE_PAGE,     mozilla_modify, mozilla_get_info },
		{ Pt_ARG_MOZ_RELOAD,            mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_STOP,              mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_PRINT,             mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_COMMAND,           mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_OPTION,           	mozilla_modify, mozilla_get_info },
		{ Pt_ARG_MOZ_GET_CONTEXT,      	NULL,						mozilla_get_info },
		{ Pt_ARG_MOZ_GET_HISTORY,      	NULL,						mozilla_get_info },
		{ Pt_ARG_MOZ_ENCODING,         	mozilla_modify, mozilla_get_info },
		{ Pt_ARG_MOZ_WEB_DATA_URL,     	mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_WEB_DATA,         	mozilla_modify, Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_UNKNOWN_RESP,      mozilla_modify,	Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_DOWNLOAD,          mozilla_modify,	Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_AUTH_CTRL,					NULL, NULL, Pt_ARG_IS_POINTER( PtMozillaWidget_t, moz_auth_ctrl ) },
		{ Pt_ARG_MOZ_UNKNOWN_CTRL,			NULL, NULL, Pt_ARG_IS_POINTER( PtMozillaWidget_t, moz_unknown_ctrl ) },
		{ Pt_CB_MOZ_INFO,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, info_cb) },
		{ Pt_CB_MOZ_START,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, start_cb) },
		{ Pt_CB_MOZ_COMPLETE,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, complete_cb) },
		{ Pt_CB_MOZ_PROGRESS,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, progress_cb) },
		{ Pt_CB_MOZ_URL,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, url_cb) },
		{ Pt_CB_MOZ_EVENT,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, event_cb) },
		{ Pt_CB_MOZ_NET_STATE,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, net_state_cb) },
		{ Pt_CB_MOZ_NEW_WINDOW,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, new_window_cb) },
		{ Pt_CB_MOZ_NEW_AREA,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, resize_cb) },
		{ Pt_CB_MOZ_DESTROY,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, destroy_cb) },
		{ Pt_CB_MOZ_VISIBILITY,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, visibility_cb) },
		{ Pt_CB_MOZ_OPEN,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, open_cb) },
		{ Pt_CB_MOZ_DIALOG,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, dialog_cb) },
		{ Pt_CB_MOZ_AUTHENTICATE,		NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, auth_cb) },
		{ Pt_CB_MOZ_PROMPT,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, prompt_cb) },
		{ Pt_CB_MOZ_CONTEXT,			NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, context_cb) },
		{ Pt_CB_MOZ_PRINT_STATUS,		NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, print_status_cb) },
		{ Pt_CB_MOZ_WEB_DATA_REQ,		NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, web_data_req_cb) },
		{ Pt_CB_MOZ_UNKNOWN,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, web_unknown_cb) },
		{ Pt_CB_MOZ_ERROR,					NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, web_error_cb) },
		{ Pt_CB_MOZ_DOWNLOAD,				NULL, NULL, Pt_ARG_IS_CALLBACK_LIST(PtMozillaWidget_t, web_download_cb) }
	};

	static const PtArg_t args[] = 
	{
		{ Pt_SET_VERSION, 200},
		{ Pt_SET_STATE_LEN, sizeof( PtMozillaWidget_t ) },
		{ Pt_SET_DFLTS_F, (long)mozilla_defaults },
		{ Pt_SET_EXTENT_F, (long)mozilla_extent },
		{ Pt_SET_FLAGS, Pt_RECTANGULAR, Pt_RECTANGULAR },
		{ Pt_SET_DESTROY_F, (long) mozilla_destroy },
		{ Pt_SET_CHILD_GETTING_FOCUS_F, ( long ) child_getting_focus },
		{ Pt_SET_CHILD_LOSING_FOCUS_F, ( long ) child_losing_focus },
		{ Pt_SET_RESOURCES, (long) resources },
		{ Pt_SET_NUM_RESOURCES, sizeof( resources )/sizeof( resources[0] ) },
		{ Pt_SET_DESCRIPTION, (long) "PtMozilla" },
	};

	if (StartupEmbedding() == -1)
		return (NULL);

	Init_nsUnknownContentTypeHandler_Factory( );

	nsCOMPtr<nsIFactory> promptFactory;
	NS_NewPromptServiceFactory(getter_AddRefs(promptFactory));

  nsCOMPtr<nsIComponentRegistrar> registrar;
  NS_GetComponentRegistrar(getter_AddRefs(registrar));
  registrar->RegisterFactory(kPromptServiceCID, "Prompt Service", 
			"@mozilla.org/embedcomp/prompt-service;1",
			promptFactory);
	PtMozilla->wclass = PtCreateWidgetClass(PtContainer, 0, sizeof(args)/sizeof(args[0]), args);

	return (PtMozilla->wclass);
}
