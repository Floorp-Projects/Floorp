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
 *   Ramiro Estrugo <ramiro@eazel.com>
 *	 Brian Edmond <briane@qnx.com>
 */
#include <stdlib.h>

#include "nsCWebBrowser.h"
#include "nsILocalFile.h"
#include "nsISelectionController.h"
#include "nsEmbedAPI.h"

#include "nsWidgetsCID.h"
#include "nsIAppShell.h"

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

#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsUnknownContentTypeHandler.h"

#include "EmbedPrivate.h"
#include "EmbedWindow.h"
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

int MozSavePageAs(PtWidget_t *widget, char *fname, int type)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	char dirname[1024];

	if (!fname || !widget) return -1;

	sprintf( dirname, "%s.dir", fname );
	moz->EmbedRef->SaveAs( fname, dirname );

	return (0);
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
//JPB
	moz->disable_new_windows = 0;
	moz->disable_exception_dlg = 0;

	// widget related
	basic->flags = Pt_ALL_OUTLINES | Pt_ALL_BEVELS | Pt_FLAT_FILL;
	widget->resize_flags &= ~Pt_RESIZE_XY_BITS; // fixed size.
	widget->anchor_flags = Pt_TOP_ANCHORED_TOP | Pt_LEFT_ANCHORED_LEFT | \
			Pt_BOTTOM_ANCHORED_TOP | Pt_RIGHT_ANCHORED_LEFT | Pt_ANCHORS_INVALID;

	cntr->flags |= Pt_CHILD_GETTING_FOCUS;
}

// widget destroy function
static void 
mozilla_destroy( PtWidget_t *widget ) 
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	if (moz->EmbedRef)
	{
		if (moz->download_dest)
			moz->EmbedRef->CancelSaveURI();

		moz->EmbedRef->Destroy();
		delete moz->EmbedRef;
	}
}

static int child_getting_focus( PtWidget_t *widget, PtWidget_t *child, PhEvent_t *ev ) {
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	nsCOMPtr<nsPIDOMWindow> piWin;
	moz->EmbedRef->GetPIDOMWindow( getter_AddRefs( piWin ) );
	if( !piWin ) return Pt_CONTINUE;

	nsCOMPtr<nsIFocusController> focusController;
	piWin->GetRootFocusController(getter_AddRefs(focusController));
	if( focusController ) focusController->SetActive( PR_TRUE );
	return Pt_CONTINUE;
	}

// realized
static void 
mozilla_realized( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	PtSuperClassRealized(PtContainer, widget);

	moz->EmbedRef->Show();

	// If an initial url was stored, load it
  	if (moz->url[0])
		MozLoadURL(moz, moz->url);
}


// unrealized function
static void 
mozilla_unrealized( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;

	moz->EmbedRef->Hide();
}

static PpPrintContext_t *moz_construct_print_context( WWWRequest *pPageInfo ) {
	PpPrintContext_t *pc = NULL;
	int i;
	char *tmp;
	pc = PpPrintCreatePC();

	i = 0;
	while( pPageInfo->Print.data[i] == '\0' ) i++;
	tmp = &pPageInfo->Print.data[i];


	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_NAME, ( pPageInfo->Print.name != -1 ) ? &tmp[pPageInfo->Print.name]: NULL);

	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_LOCATION, ( pPageInfo->Print.location != -1 ) ? &tmp[pPageInfo->Print.location]: NULL);
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_DEVICE, ( pPageInfo->Print.device != -1 ) ? &tmp[pPageInfo->Print.device]: NULL );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_FILTER, ( pPageInfo->Print.filter != -1 ) ? &tmp[pPageInfo->Print.filter]: NULL );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_PREVIEW_APP, ( pPageInfo->Print.preview_app != -1 ) ? &tmp[pPageInfo->Print.preview_app]: NULL );

	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_FILENAME, ( pPageInfo->Print.filename != -1 ) ? &tmp[pPageInfo->Print.filename]: NULL );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_COMMENT, ( pPageInfo->Print.comment != -1 ) ? &tmp[pPageInfo->Print.comment]: NULL );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_DATE, ( pPageInfo->Print.date != -1 ) ? &tmp[pPageInfo->Print.date]: NULL );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_USER_ID, ( pPageInfo->Print.user_id != -1 ) ? &tmp[pPageInfo->Print.user_id]: NULL );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_PAGE_RANGE, &pPageInfo->Print.page_range );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_DO_PREVIEW, &pPageInfo->Print.do_preview );

	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_NONPRINT_MARGINS, &pPageInfo->Print.non_printable_area );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_INTENSITY, &pPageInfo->Print.intensity );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_PRINTER_RESOLUTION, &pPageInfo->Print.printer_resolution );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_PAPER_SIZE, &pPageInfo->Print.paper_size );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_COLLATING_MODE, &pPageInfo->Print.collating_mode );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_DITHERING, &pPageInfo->Print.dithering );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_COPIES, &pPageInfo->Print.copies );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_ORIENTATION, &pPageInfo->Print.orientation );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_DUPLEX, &pPageInfo->Print.duplex );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_PAPER_TYPE, &pPageInfo->Print.paper_type );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_PAPER_SOURCE, &pPageInfo->Print.paper_source );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_INKTYPE, &pPageInfo->Print.inktype );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_COLOR_MODE, &pPageInfo->Print.color_mode );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_MARGINS, &pPageInfo->Print.margins );
	PpPrintSetPC( pc, INITIAL_PC, 0, Pp_PC_MAX_DEST_SIZE, &pPageInfo->Print.max_dest_size );
	tmp = pc->control.tmp_target;
	pc->control = pPageInfo->Print.control;
	pc->control.tmp_target = tmp;
	return pc;
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
mozilla_modify( PtWidget_t *widget, PtArg_t const *argt ) 
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
				if (argt->value == WWW_DIRECTION_FWD)
					moz->EmbedRef->Forward();
				else if (argt->value == WWW_DIRECTION_BACK)
					moz->EmbedRef->Back();
				else 
				{
					PhDim_t dim;

					PtWidgetDim(widget, &dim);
					dim.w = (argt->value * dim.w)/100;
					dim.h = (argt->value * dim.h)/100;

					if (argt->value == WWW_DIRECTION_UP)
						moz->EmbedRef->ScrollUp(dim.h);
					else if (argt->value ==	WWW_DIRECTION_DOWN)
						moz->EmbedRef->ScrollDown(dim.h);
					else if (argt->value ==	WWW_DIRECTION_LEFT)
						moz->EmbedRef->ScrollLeft(dim.w);
					else if (argt->value == WWW_DIRECTION_RIGHT)
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
			WWWRequest *pPageInfo = ( WWWRequest * ) argt->value;
			PpPrintContext_t *pc = moz_construct_print_context( pPageInfo );
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
 			pref->SetUnicharPref( "intl.charset.default", NS_ConvertASCIItoUCS2( translation ).get());
			pref->SavePrefFile( nsnull );
			}
			break;

		case Pt_ARG_MOZ_COMMAND:
			switch ((int)(argt->value)) 
			{
				case Pt_MOZ_COMMAND_CUT: {
					moz->EmbedRef->Cut();
					}
					break;
				case Pt_MOZ_COMMAND_COPY: {
					moz->EmbedRef->Copy();
					}
					break;
				case Pt_MOZ_COMMAND_PASTE: {
					moz->EmbedRef->Paste();
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
					PtWebCommand_t *wdata = ( PtWebCommand_t * ) argt->len;
					nsCOMPtr<nsIWebBrowserFind> finder( do_GetInterface( moz->EmbedRef->mWindow->mWebBrowser ) );
					finder->SetSearchString( NS_ConvertASCIItoUCS2(wdata->FindInfo.szString).get() );
					finder->SetMatchCase( wdata->FindInfo.flags & FINDFLAG_MATCH_CASE );
					finder->SetFindBackwards( wdata->FindInfo.flags & FINDFLAG_GO_BACKWARDS );
					finder->SetWrapFind( wdata->FindInfo.flags & FINDFLAG_START_AT_TOP );
//				finder->SetEntireWord( entireWord ); /* not defined in voyager */

					PRBool didFind;
					finder->FindNext( &didFind );
					break;
					}
			}
			break;

		case Pt_ARG_MOZ_WEB_DATA_URL:
#if 0
//				moz->MyBrowser->WebBrowserContainer->OpenStream( moz->MyBrowser->WebBrowser, "file://", "text/html" );
//				strcpy( moz->url, (char*)argt->value );
#endif
				break;

		case Pt_ARG_MOZ_UNKNOWN_RESP: 
#if 1
			{
				PtMozUnknownResp_t *data = ( PtMozUnknownResp_t * ) argt->value;
				switch( data->response ) 
				{
					case WWW_RESPONSE_OK:
						if( moz->EmbedRef->app_launcher ) {
							if( moz->download_dest ) free( moz->download_dest );
							moz->download_dest = strdup( data->filename );
							moz->EmbedRef->app_launcher->SaveToDisk( NULL, PR_TRUE );
							}
						break;
					case WWW_RESPONSE_CANCEL: {
							moz->EmbedRef->app_launcher->Cancel( );
							moz->EmbedRef->app_launcher->CloseProgressWindow( );
							moz->EmbedRef->app_launcher = NULL;
							}
						break;
				}
			}
#endif
			break;

		case Pt_ARG_MOZ_DOWNLOAD: 
			{
				if( moz->download_dest ) 
					free( moz->download_dest );
				moz->download_dest = strdup( (char*)argt->len );
				moz->EmbedRef->SaveURI((char*)argt->value, (char*) argt->len);
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
					case WWW_DATA_HEADER:
						if( !moz->MyBrowser->WebBrowserContainer->IsStreaming( ) ) break;

						/* request the document body */
						if( moz->web_data_req_cb ) {
							PtCallbackInfo_t  cbinfo;
							PtMozillaDataRequestCb_t cb;

							memset( &cbinfo, 0, sizeof( cbinfo ) );
							cbinfo.reason = Pt_CB_MOZ_WEB_DATA_REQ;
							cbinfo.cbdata = &cb;
							cb.type = WWW_DATA_BODY;
							cb.length = 32768;
							cb.url = moz->url;
							PtInvokeCallbackList( moz->web_data_req_cb, (PtWidget_t *)moz, &cbinfo);
							}
						break;
					case WWW_DATA_BODY: {
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
						cb.type = len ? WWW_DATA_BODY : WWW_DATA_CLOSE;
						cb.length = 32768;
						cb.url = moz->url;
						PtInvokeCallbackList( moz->web_data_req_cb, (PtWidget_t *)moz, &cbinfo);
						}
						break;
					case WWW_DATA_CLOSE:
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
mozilla_get_info(PtWidget_t *widget, PtArg_t *argt)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	nsIPref *pref = moz->EmbedRef->GetPrefs();

	switch (argt->type)
	{
		case Pt_ARG_MOZ_NAVIGATE_PAGE:
			moz->navigate_flags = 0;
			if (moz->EmbedRef->CanGoBack())
				moz->navigate_flags |= ( 1 << WWW_DIRECTION_BACK );
			if (moz->EmbedRef->CanGoForward())
				moz->navigate_flags |= ( 1 << WWW_DIRECTION_FWD );
			*((int **)argt->value) = &moz->navigate_flags;
			break;

		case Pt_ARG_MOZ_OPTION:
			mozilla_get_pref( widget, (char*)argt->len, (char*) argt->value );
			break;

		case Pt_ARG_MOZ_GET_CONTEXT:
			if ( moz->rightClickUrl ) 
				strcpy( (char*) argt->value, moz->rightClickUrl );
			else 
				*(char*) argt->value = 0;
			break;

		case Pt_ARG_MOZ_ENCODING: 
			{
			PRUnichar *charset = nsnull;
			pref->GetLocalizedUnicharPref( "intl.charset.default", &charset );

			strcpy( (char*)argt->value, NS_ConvertUCS2toUTF8(charset).get() );
			}
			break;

		case Pt_ARG_MOZ_GET_HISTORY: 
			{
				WWWRequest *req = ( WWWRequest * ) argt->len;
				PtWebClientHistoryData_t *HistoryReplyBuf = (PtWebClientHistoryData_t *) argt->value;
				int i, j;
				PRInt32 total;
				moz->EmbedRef->mSessionHistory->GetCount( &total );

				for( i=total-2, j=0; i>=0 && j<req->History.num; i--, j++) 
				{
					nsIHistoryEntry *entry;
					moz->EmbedRef->mSessionHistory->GetEntryAtIndex( i, PR_FALSE, &entry );
				
					PRUnichar *title;
					nsIURI *url;
					entry->GetTitle( &title );
					entry->GetURI( &url );

					nsString stitle( title );
					strncpy( HistoryReplyBuf[j].title, ToNewCString(stitle), 127 );
					HistoryReplyBuf[j].title[127] = '\0';

					nsCAutoString specString;
					url->GetSpec(specString);
					REMOVE_WHEN_NEW_PT_WEB_strcpy( HistoryReplyBuf[j].url, specString.get() );
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
		pref->SetUnicharPref( "browser.visited_color", NS_ConvertASCIItoUCS2(value).get() );
		}
	else if( !strcmp( option, "A:link color" ) ) {
		pref->SetUnicharPref( "browser.anchor_color", NS_ConvertASCIItoUCS2(value).get() );
		}

/* the mozserver already has A:link color == browser.anchor_color for this */
//	else if( !strcmp( option, "A:active color" ) ) {
//		pref->SetUnicharPref( "browser.anchor_color", NS_ConvertASCIItoUCS2(value).get() );
//		}

	else if( !strcmp( option, "BODY color" ) ) {
		pref->SetUnicharPref( "browser.display.foreground_color", NS_ConvertASCIItoUCS2(value).get() );
		}
	else if( !strcmp( option, "BODY background" ) ) {
		pref->SetUnicharPref( "browser.display.background_color", NS_ConvertASCIItoUCS2(value).get() );
		}
	else if( !strcmp( option, "bIgnoreDocumentAttributes" ) )
		pref->SetBoolPref( "browser.display.use_document_colors", stricmp( value, "TRUE" ) ? PR_FALSE : PR_TRUE );
	else if( !strcmp( option, "bUnderlineLinks" ) ) {
		pref->SetBoolPref( "browser.underline_anchors", !stricmp( value, "TRUE" ) ? PR_TRUE : PR_FALSE );
		}
	else if( !strcmp( option, "iUserTextSize" ) ) {
		int n = atoi( value );
		/* map our n= 0...4 value into a mozilla font value */
		/* see xpfe/components/prefwindow/resources/content/pref-fonts.xul, they are also hard-coded there */
		switch( n ) {
			case 0: n = VOYAGER_TEXTSIZE0; break;
			case 1: n = VOYAGER_TEXTSIZE1; break;
			case 2: n = VOYAGER_TEXTSIZE2; break;
			case 3: n = VOYAGER_TEXTSIZE3; break;
			case 4: n = VOYAGER_TEXTSIZE4; break;
			}
//		moz->MyBrowser->mPrefs->SetIntPref( "font.size.fixed.x-western", n );
		pref->SetIntPref( "font.size.variable.x-western", n );
		}
	else if( !strcmp( option, "BODY font-family" ) ) {
		/* set the current font */
		char *font_default = NULL;
		char preference[256];

		pref->CopyCharPref( "font.default", &font_default );
		if( !font_default ) font_default = "serif";

		sprintf( preference, "font.name.%s.x-western", font_default );
		pref->SetCharPref( preference, value );
		}
	else if( !strcmp( option, "PRE font-family" ) || !strcmp( option, "H* font-family" ) ) {
		/* do not set these - use the BODY font-family instead */
		}

/* SOCKS options */
	else if( !strcmp( option, "socks_server" ) )
		pref->SetCharPref( "network.hosts.socks_server", value );
	else if( !strcmp( option, "socks_port" ) )
		pref->SetCharPref( "network.hosts.socks_serverport", value );
	else if( !strcmp( option, "socks_user" ) ) 		; /* not used */
	else if( !strcmp( option, "socks_app" ) ) 		; /* not used */

/* HTTP options */
	else if( !strcmp( option, "http_proxy_host" ) )
		pref->SetCharPref( "network.proxy.http", value );
	else if( !strcmp( option, "http_proxy_port" ) )
		pref->SetCharPref( "network.proxy.http_port", value );
	else if( !strcmp( option, "proxy_overrides" ) )
		pref->SetCharPref( "network.proxy.no_proxies_on", value );

/* FTP options */
	else if( !strcmp( option, "ftp_proxy_host" ) )
		pref->SetCharPref( "network.proxy.ftp", value );
	else if( !strcmp( option, "ftp_proxy_port" ) )
		pref->SetCharPref( "network.proxy.ftp_port", value );
	else if( !strcmp( option, "email_address" ) )		; /* not used */

/* Gopher options */
	else if( !strcmp( option, "gopher_proxy_host" ) )
		pref->SetCharPref( "network.proxy.gopher", value );
	else if( !strcmp( option, "gopher_proxy_port" ) )
		pref->SetCharPref( "network.proxy.gopher_port", value );

/* TCP/IP options */
	else if( !strcmp( option, "socket_timeout" ) )
		pref->SetIntPref( "network.http.connect.timeout", atoi( value ) );
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
	else if( !strcmp( option, "Print_Left_Header_String" ) ) 		; /* not used */
	else if( !strcmp( option, "Print_Right_Header_String" ) ) 		; /* not used */
	else if( !strcmp( option, "Print_Left_Footer_String" ) ) 		; /* not used */
	else if( !strcmp( option, "Print_Right_Footer_String" ) ) 		; /* not used */


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

	pref->SavePrefFile( nsnull );
	}

static void mozilla_get_pref( PtWidget_t *widget, char *option, char *value ) {
	PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) widget;
	nsIPref *pref = moz->EmbedRef->GetPrefs();

	/* HTML Options */
	if( !strcmp( option, "A:link color" ) || !strcmp( option, "A:active color" ) ) {
		nsXPIDLCString colorStr;
		pref->CopyCharPref( "browser.anchor_color", getter_Copies(colorStr) );
		strcpy( value, colorStr );
		}
	else if( !strcmp( option, "A:visited color" ) ) {
		nsXPIDLCString colorStr;
		pref->CopyCharPref( "browser.visited_color", getter_Copies(colorStr) );
		strcpy( value, colorStr );
		}
	else if( !strcmp( option, "BODY color" ) ) {
		nsXPIDLCString colorStr;
		pref->CopyCharPref( "browser.display.foreground_color", getter_Copies(colorStr) );
		strcpy( value, colorStr );
		}
	else if( !strcmp( option, "BODY background" ) ) {
		nsXPIDLCString colorStr;
		pref->CopyCharPref( "browser.display.background_color", getter_Copies(colorStr) );
		strcpy( value, colorStr );
		}
	else if( !strcmp( option, "bIgnoreDocumentAttributes" ) ) {
		PRBool val;
		pref->GetBoolPref( "browser.display.use_document_colors", &val );
		sprintf( value, "%s", val == PR_TRUE ? "FALSE" : "TRUE" );
		}
//JPB
	else if( !strcmp( option, "disable_new_windows" ) ) {
		sprintf( value, "%s", (moz->disable_new_windows == 1) ? "TRUE" : "FALSE" );
		}
	else if( !strcmp( option, "disable_exception_dlg" ) ) {
		sprintf( value, "%s", (moz->disable_exception_dlg == 1) ? "TRUE" : "FALSE" );
		}
//JPB
	else if( !strcmp( option, "bUnderlineLinks" ) ) {
		PRBool val;
		pref->GetBoolPref( "browser.underline_anchors", &val );
		sprintf( value, "%s", val == PR_TRUE ? "TRUE" : "FALSE" );
		}
	else if( !strcmp( option, "iUserTextSize" ) ) {
		int n;
//		moz->MyBrowser->mPrefs->GetIntPref( "font.size.fixed.x-western", &n );
		pref->GetIntPref( "font.size.variable.x-western", &n );
		/* map our n= 0...4 value into a mozilla font value */
		/* see xpfe/components/prefwindow/resources/content/pref-fonts.xul, they are also hard-coded there */
		switch( n ) {
			case VOYAGER_TEXTSIZE0: n = 0; break;
			case VOYAGER_TEXTSIZE1: n = 1; break;
			case VOYAGER_TEXTSIZE2: n = 2; break;
			case VOYAGER_TEXTSIZE3: n = 3; break;
			case VOYAGER_TEXTSIZE4: n = 4; break;
			}
		sprintf( value, "%d", n );
		}
	else if( !strcmp( option, "BODY font-family" ) || !strcmp( option, "PRE font-family" ) || !strcmp( option, "H* font-family" ) ) {
		/* set the current font */
		char *font_default = NULL, *font = NULL;
		char preference[256];

		pref->CopyCharPref( "font.default", &font_default );
		if( !font_default ) font_default = "serif";

		sprintf( preference, "font.name.%s.x-western", font_default );
		pref->CopyCharPref( preference, &font );
		strcpy( value, font );
		}

/* SOCKS options */
  else if( !strcmp( option, "socks_server" ) ) {
		char *s = NULL;
		pref->CopyCharPref( "network.hosts.socks_server", &s );
		if( s ) strcpy( value, s );
		}
  else if( !strcmp( option, "socks_port" ) ) {
		char *s = NULL;
		pref->CopyCharPref( "network.hosts.socks_serverport", &s );
		if( s ) strcpy( value, s );
		}

/* HTTP options */
  else if( !strcmp( option, "http_proxy_host" ) ) {
		char *s = NULL;
		pref->CopyCharPref( "network.proxy.http", &s );
		if( s ) strcpy( value, s );
		}
  else if( !strcmp( option, "http_proxy_port" ) ) {
		char *s = NULL;
		pref->CopyCharPref( "network.proxy.http_port", &s );
		if( s ) strcpy( value, s );
		}
  else if( !strcmp( option, "proxy_overrides" ) ) {
		char *s = NULL;
		pref->CopyCharPref( "network.proxy.no_proxies_on", &s );
		if( s ) strcpy( value, s );
		}

/* FTP options */
  else if( !strcmp( option, "ftp_proxy_host" ) ) {
		char *s = NULL;
		pref->CopyCharPref( "network.proxy.ftp", &s );
		if( s ) strcpy( value, s );
		}
  else if( !strcmp( option, "ftp_proxy_port" ) ) {
		char *s = NULL;
		pref->CopyCharPref( "network.proxy.ftp_port", &s );
		if( s ) strcpy( value, s );
		}

/* Gopher options */
  else if( !strcmp( option, "gopher_proxy_host" ) ) {
		char *s = NULL;
		pref->CopyCharPref( "network.proxy.gopher", &s );
		if( s ) strcpy( value, s );
		}
  else if( !strcmp( option, "gopher_proxy_port" ) ) {
		char *s = NULL;
		pref->CopyCharPref( "gopher_proxy_port", &s );
		if( s ) strcpy( value, s );
		}

/* TCP/IP options */
  else if( !strcmp( option, "socket_timeout" ) ) {
		int n;
		pref->GetIntPref( "network.http.connect.timeout", &n );
		sprintf( value, "%d", n );
		}
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
	else *value = 0;

	}

/* static */
static int
StartupProfile(char *sProfileDir, char *sProfileName)
{
  	// initialize profiles
  	if (sProfileDir && sProfileName) 
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
		  	if NS_FAILED(rv)
				return NS_ERROR_FAILURE;
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
static int
StartupEmbedding()
{
    nsresult rv;
	char *profile_dir;

#ifdef _BUILD_STATIC_BIN
  // Initialize XPCOM's module info table
  NSGetStaticModuleInfo = ph_getModuleInfo;
#endif
    
    rv = NS_InitEmbedding(nsnull, nsnull);
    if (NS_FAILED(rv))
      return (-1);

	profile_dir = (char *)alloca(strlen(getenv("HOME")) + strlen("/.ph") + 1);
	sprintf(profile_dir, "%s/.ph", getenv("HOME"));
    rv = StartupProfile(profile_dir, "mozilla");
    if (NS_FAILED(rv))
      	NS_WARNING("Warning: Failed to start up profiles.\n");
    
    nsCOMPtr<nsIAppShell> appShell;
    appShell = do_CreateInstance(kAppShellCID);
    if (!appShell) 
	{
	    NS_WARNING("Failed to create appshell in EmbedPrivate::PushStartup!\n");
    	return (-1);
    }
    nsIAppShell * sAppShell = appShell.get();
    NS_ADDREF(sAppShell);
    sAppShell->Create(0, nsnull);
    sAppShell->Spinup();

	return (0);
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
		{ Pt_ARG_MOZ_DOWNLOAD,					mozilla_modify,	Pt_QUERY_PREVENT },
		{ Pt_ARG_MOZ_AUTH_CTRL,					NULL, NULL, Pt_ARG_IS_POINTER( PtMozillaWidget_t, moz_auth_ctrl ) },
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
	};

	static const PtArg_t args[] = 
	{
		{ Pt_SET_VERSION, 200},
		{ Pt_SET_STATE_LEN, sizeof( PtMozillaWidget_t ) },
		{ Pt_SET_DFLTS_F, (long)mozilla_defaults },
		{ Pt_SET_REALIZED_F, (long)mozilla_realized },
		{ Pt_SET_UNREALIZE_F, (long)mozilla_unrealized },
		{ Pt_SET_EXTENT_F, (long)mozilla_extent },
		{ Pt_SET_FLAGS, Pt_RECTANGULAR, Pt_RECTANGULAR },
		{ Pt_SET_DESTROY_F, (long) mozilla_destroy },
		{ Pt_SET_CHILD_GETTING_FOCUS_F, ( long ) child_getting_focus },
		{ Pt_SET_RESOURCES, (long) resources },
		{ Pt_SET_NUM_RESOURCES, sizeof( resources )/sizeof( resources[0] ) },
		{ Pt_SET_DESCRIPTION, (long) "PtMozilla" },
	};

	if (StartupEmbedding() == -1)
		return (NULL);

	Init_nsUnknownContentTypeHandler_Factory( );

	nsCOMPtr<nsIFactory> promptFactory;
	NS_NewPromptServiceFactory(getter_AddRefs(promptFactory));
	nsComponentManager::RegisterFactory(kPromptServiceCID, "Prompt Service", 
			"@mozilla.org/embedcomp/prompt-service;1",
			promptFactory, PR_TRUE); // replace existing
	PtMozilla->wclass = PtCreateWidgetClass(PtContainer, 0, sizeof(args)/sizeof(args[0]), args);

	return (PtMozilla->wclass);
}
