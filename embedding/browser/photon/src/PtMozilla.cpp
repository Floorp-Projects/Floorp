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
#include "PtMozilla.h"

#include <photon/PtWebClient.h>
#include <photon/PpProto.h>

#include "nsCWebBrowser.h"
#include "nsFileSpec.h"
#include "nsILocalFile.h"
#include "nsIContentViewerFile.h"
#include "nsISelectionController.h"
#include "nsEmbedAPI.h"

#include "nsAppShellCIDs.h"
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"

#include "nsIDocumentLoaderFactory.h"
#include "nsILoadGroup.h"

#include "nsIWebBrowserFind.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWebBrowserPrint.h"
#include "nsIPrintOptions.h"

#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDiskDocument.h"
#include "nsIDOMWindow.h"
#include "nsNetUtil.h"
#include "nsMPFileLocProvider.h"
#include "nsIFocusController.h"
#include "PromptService.h"

#include "nsReadableUtils.h"

#include "nsIViewManager.h"
#include "nsIPresShell.h"

#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIURIFixup.h"

#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"

#include "nsUnknownContentTypeHandler.h"

// Macro for converting from nscolor to PtColor_t
// Photon RGB values are stored as 00 RR GG BB
// nscolor RGB values are 00 BB GG RR
#define NS_TO_PH_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)
#define PH_TO_NS_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

#define NS_EXTERN_IID(_name) \
        extern const nsIID _name;

        // Class IDs
        NS_EXTERN_IID(kHTMLEditorCID);
        NS_EXTERN_IID(kCookieServiceCID);
        NS_EXTERN_IID(kWindowCID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kSimpleURICID,            NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);

#define NS_PROMPTSERVICE_CID \
 {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}
static NS_DEFINE_CID(kPromptServiceCID, NS_PROMPTSERVICE_CID);

static void mozilla_set_pref( PtWidget_t *widget, char *option, char *value );
static void mozilla_get_pref( PtWidget_t *widget, char *option, char *value );

PtWidgetClass_t *PtCreateMozillaClass( void );
#ifndef _PHSLIB
	PtWidgetClassRef_t __PtMozilla = { NULL, PtCreateMozillaClass };
	PtWidgetClassRef_t *PtMozilla = &__PtMozilla; 
#endif


static void MozCreateWindow(PtMozillaWidget_t *moz)
{
	nsresult rv;

	// Create web shell
	moz->MyBrowser->WebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
	if( !moz->MyBrowser->WebBrowser )
	{
		printf("Web shell creation failed\n");
		exit(-1);
	}


		// Create the container object
		moz->MyBrowser->WebBrowserContainer = new CWebBrowserContainer((PtWidget_t *)moz);
		if (moz->MyBrowser->WebBrowserContainer == NULL) {
			printf("Could not create webbrowsercontainer\n");
			exit(-1);
			}
		moz->MyBrowser->WebBrowserContainer->AddRef();


		moz->MyBrowser->WebBrowser->SetContainerWindow( NS_STATIC_CAST( nsIWebBrowserChrome*, moz->MyBrowser->WebBrowserContainer ) );
		//nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(moz->MyBrowser->WebBrowser);
		//dsti->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

    // Create the webbrowser window
		moz->MyBrowser->WebBrowserAsWin = do_QueryInterface(moz->MyBrowser->WebBrowser);
		rv = moz->MyBrowser->WebBrowserAsWin->InitWindow( moz /* PtWidgetParent(moz) */, nsnull, 0, 0, 200, 200 );
		rv = moz->MyBrowser->WebBrowserAsWin->Create();

		// Configure what the web browser can and cannot do
		nsCOMPtr<nsIWebBrowserSetup> webBrowserAsSetup(do_QueryInterface(moz->MyBrowser->WebBrowser));
		// webBrowserAsSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_PLUGINS, aAllowPlugins);
		// webBrowserAsSetup->SetProperty(nsIWebBrowserSetup::SETUP_CONTAINS_CHROME, PR_TRUE);



		// Set up the web shell
// ATENTIE		moz->MyBrowser->WebBrowser->SetParentURIContentListener( (nsIURIContentListener*) moz );

		// XXX delete when tree owner is not necessary (to receive context menu events)
		nsCOMPtr<nsIDocShellTreeItem> browserAsItem = do_QueryInterface(moz->MyBrowser->WebBrowser);
		browserAsItem->SetTreeOwner(NS_STATIC_CAST(nsIDocShellTreeOwner *, moz->MyBrowser->WebBrowserContainer));

    // XXX delete when docshell becomes inaccessible
    moz->MyBrowser->rootDocShell = do_GetInterface(moz->MyBrowser->WebBrowser);
    if (moz->MyBrowser->rootDocShell == nsnull) {
			printf("Could not get root docshell object\n");
			exit(-1);
    	}

	nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref), (nsISupports **)&(moz->MyBrowser->mPrefs));

#if 0
	nsCOMPtr<nsIWebProgressListener> listener = NS_STATIC_CAST(nsIWebProgressListener*, moz->MyBrowser->WebBrowserContainer );
	nsCOMPtr<nsISupports> supports = do_QueryInterface(listener);
	(void)moz->MyBrowser->WebBrowser->AddWebBrowserListener(supports, NS_GET_IID(nsIWebProgressListener));
#else
	nsCOMPtr<nsIWebProgressListener> listener = NS_STATIC_CAST(nsIWebProgressListener*, moz->MyBrowser->WebBrowserContainer );
	nsCOMPtr<nsISupportsWeakReference> supportsWeak;
	supportsWeak = do_QueryInterface(listener);
	nsCOMPtr<nsIWeakReference> weakRef;
	supportsWeak->GetWeakReference(getter_AddRefs(weakRef));
	(void)moz->MyBrowser->WebBrowser->AddWebBrowserListener(weakRef, nsIWebProgressListener::GetIID());
#endif
	}



/* release whatever memory was allocated in the MozCreateWindow function */
static void MozDestroyWindow( PtMozillaWidget_t *moz ) {
	// release session history
	moz->MyBrowser->mSessionHistory = nsnull;

	moz->MyBrowser->WebBrowserAsWin->Destroy( );
	nsServiceManager::ReleaseService( kPrefCID, (nsISupports *)(moz->MyBrowser->mPrefs ) );
	}


void MozSetPreference(PtWidget_t *widget, int type, char *pref, void *data)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	switch (type)
	{
		case Pt_MOZ_PREF_CHAR:
			moz->MyBrowser->mPrefs->SetCharPref(pref, (char *)data);
			break;
		case Pt_MOZ_PREF_BOOL:
			moz->MyBrowser->mPrefs->SetBoolPref(pref, (int)data);
			break;
		case Pt_MOZ_PREF_INT:
			moz->MyBrowser->mPrefs->SetIntPref(pref, (int)data);
			break;
		case Pt_MOZ_PREF_COLOR:
// not supported yet
//			moz->MyBrowser->mPrefs->SetColorPrefDWord(pref, (uint32) data);
			break;
	}
}


int MozSavePageAs(PtWidget_t *widget, char *fname, int type)
{
	// Get the current DOM document
	nsIDOMDocument* pDocument = nsnull;
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	if (!fname || !widget)
		return (-1);

	// Get the DOM window from the webbrowser
	nsCOMPtr<nsIDOMWindow> window;
	moz->MyBrowser->WebBrowser->GetContentDOMWindow(getter_AddRefs(window));
	if (window)
	{
		if (NS_SUCCEEDED(window->GetDocument(&pDocument)))
		{
			// Get an nsIDiskDocument interface to the DOM document
			nsCOMPtr<nsIDiskDocument> diskDoc = do_QueryInterface(pDocument);
			if (!diskDoc)
				return (-1);

			// Create an nsFilelSpec from the selected file path.
			nsFileSpec fileSpec(fname, PR_FALSE);

			// Figure out the mime type from the selection
			nsAutoString mimeType;
			switch (type)
			{
				case Pt_MOZ_SAVEAS_HTML:
					mimeType.AssignWithConversion("text/html");
					break;
				case Pt_MOZ_SAVEAS_TEXT:
				default:
					mimeType.AssignWithConversion("text/plain");
					break;
			}

			// Save the file.
			nsAutoString useDocCharset;
//			diskDoc->SaveFile(&fileSpec, PR_TRUE, PR_TRUE, mimeType, useDocCharset, 0);
			return (0);
		}
	}

	return (-1);
}

static void MozLoadURL(PtMozillaWidget_t *moz, char *url)
{
  	// If the widget isn't realized, just return.
	if (!(PtWidgetFlags((PtWidget_t *)moz) & Pt_REALIZED))
		return;

	if (moz->MyBrowser->WebNavigation)
		moz->MyBrowser->WebNavigation->LoadURI(NS_ConvertASCIItoUCS2(url).get(), nsIWebNavigation::LOAD_FLAGS_NONE);
}

// defaults
static void mozilla_defaults( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	PtBasicWidget_t *basic = (PtBasicWidget_t *) widget;
	PtContainerWidget_t *cntr = (PtContainerWidget_t*) widget;

	moz->MyBrowser = new BrowserAccess();

	MozCreateWindow(moz);
	// get a hold of the navigation class
	moz->MyBrowser->WebNavigation = do_QueryInterface(moz->MyBrowser->WebBrowser);

	// Create our session history object and tell the navigation object
	// to use it.  We need to do this before we create the web browser
	// window.
	moz->MyBrowser->mSessionHistory = do_CreateInstance(NS_SHISTORY_CONTRACTID);
	moz->MyBrowser->WebNavigation->SetSessionHistory( moz->MyBrowser->mSessionHistory );

  	// widget related
	basic->flags = Pt_ALL_OUTLINES | Pt_ALL_BEVELS | Pt_FLAT_FILL;
	widget->resize_flags &= ~Pt_RESIZE_XY_BITS; // fixed size.
	widget->anchor_flags = Pt_TOP_ANCHORED_TOP | Pt_LEFT_ANCHORED_LEFT | \
			Pt_BOTTOM_ANCHORED_TOP | Pt_RIGHT_ANCHORED_LEFT | Pt_ANCHORS_INVALID;

	cntr->flags |= Pt_CHILD_GETTING_FOCUS;
}

static void mozilla_destroy( PtWidget_t *widget ) {
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	MozDestroyWindow( moz );
	delete moz->MyBrowser;
	}

#if 0
static int child_getting_focus( PtWidget_t *widget, PtWidget_t *child, PhEvent_t *ev ) {
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

/* ATENTIE */ printf( "!!!!!!!!!!!child_getting_focus\n\n\n" );

//	PtSuperClassChildGettingFocus( PtContainer, widget, child, ev );

	nsCOMPtr<nsPIDOMWindow> piWin;
	moz->MyBrowser->WebBrowserContainer->GetPIDOMWindow( getter_AddRefs( piWin ) );
	if( !piWin ) return Pt_CONTINUE;

	piWin->Activate();

	return Pt_CONTINUE;
	}

static int child_losing_focus( PtWidget_t *widget, PtWidget_t *child, PhEvent_t *ev ) {
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

/* ATENTIE */ printf( "!!!!!!!!!!!!child_losing_focus\n\n\n" );

//	PtSuperClassChildLosingFocus( PtContainer, widget, child, ev );

	nsCOMPtr<nsPIDOMWindow> piWin;
	moz->MyBrowser->WebBrowserContainer->GetPIDOMWindow( getter_AddRefs( piWin ) );
	if( !piWin ) return Pt_CONTINUE;

	piWin->Deactivate();

	// but the window is still active until the toplevel gets a focus out
	nsCOMPtr<nsIFocusController> focusController;
	piWin->GetRootFocusController(getter_AddRefs(focusController));
	if( focusController ) focusController->SetActive( PR_TRUE );

	return Pt_CONTINUE;
	}

static int got_focus( PtWidget_t *widget, PhEvent_t *event ) {
  PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

/* ATENTIE */ printf( ">>>>>>>>>>>>>>>>>>>got_focus\n\n\n" );

  PtSuperClassGotFocus( PtContainer, widget, event );

  nsCOMPtr<nsPIDOMWindow> piWin;
  moz->MyBrowser->WebBrowserContainer->GetPIDOMWindow( getter_AddRefs( piWin ) );
  if( !piWin ) return Pt_CONTINUE;

  nsCOMPtr<nsIFocusController> focusController;
  piWin->GetRootFocusController( getter_AddRefs( focusController ) );
  if( focusController ) focusController->SetActive( PR_TRUE );

  return Pt_CONTINUE;
  }

static int lost_focus( PtWidget_t *widget, PhEvent_t *event ) {
  PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

/* ATENTIE */ printf( "!!!!!!!!!!!!!!!!lost_focus\n\n\n" );

  PtSuperClassLostFocus( PtContainer, widget, event );

  nsCOMPtr<nsPIDOMWindow> piWin;
  moz->MyBrowser->WebBrowserContainer->GetPIDOMWindow( getter_AddRefs( piWin ) );
  if( !piWin ) return Pt_CONTINUE;

  nsCOMPtr<nsIFocusController> focusController;
  piWin->GetRootFocusController( getter_AddRefs( focusController ) );
  if( focusController ) focusController->SetActive( PR_FALSE );

  return Pt_CONTINUE;
  }

#endif

static int child_getting_focus( PtWidget_t *widget, PtWidget_t *child, PhEvent_t *ev ) {
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	nsCOMPtr<nsPIDOMWindow> piWin;
	moz->MyBrowser->WebBrowserContainer->GetPIDOMWindow( getter_AddRefs( piWin ) );
	if( !piWin ) return Pt_CONTINUE;

	nsCOMPtr<nsIFocusController> focusController;
	piWin->GetRootFocusController(getter_AddRefs(focusController));
	if( focusController ) focusController->SetActive( PR_TRUE );

	return Pt_CONTINUE;
	}


/* this callback is invoked when the user changes video modes - it will reallocate the off screen memory */
static int mozilla_ev_info( PtWidget_t *widget, PhEvent_t *event ) {

	if( event && event->type == Ph_EV_INFO && event->subtype == Ph_OFFSCREEN_INVALID ) {
		PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

		nsCOMPtr<nsIPresShell> presShell;
		moz->MyBrowser->rootDocShell->GetPresShell( getter_AddRefs(presShell) );

		nsCOMPtr<nsIViewManager> viewManager;
		presShell->GetViewManager(getter_AddRefs(viewManager));
		NS_ENSURE_TRUE(viewManager, NS_ERROR_FAILURE);

// find some other way to recreate the off screen surface		viewManager->DestroyDrawingSurface( ); /* this will force the reallocation of offscreen context */

		PtDamageWidget( widget );
		}

	return Pt_CONTINUE;
	}


// realized
static void mozilla_realized( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	PtSuperClassRealized(PtContainer, widget);

	moz->MyBrowser->WebBrowserAsWin->SetVisibility(PR_TRUE);

	// If an initial url was stored, load it
  	if (moz->url[0])
		MozLoadURL(moz, moz->url);
}


// unrealized function
static void mozilla_unrealized( PtWidget_t *widget )
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;

	moz->MyBrowser->WebBrowserAsWin->SetVisibility(PR_FALSE);
}

static PpPrintContext_t *moz_construct_print_context( WWWRequest *pPageInfo ) {
	PpPrintContext_t *pc;
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

static void mozilla_extent(PtWidget_t *widget)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;

	PtSuperClassExtent(PtContainer, widget);

	moz->MyBrowser->WebBrowserAsWin->SetPosition(widget->area.pos.x, widget->area.pos.y);
	moz->MyBrowser->WebBrowserAsWin->SetSize(widget->area.size.w, widget->area.size.h, PR_TRUE);
}

// set resources function
static void mozilla_modify( PtWidget_t *widget, PtArg_t const *argt ) {

	PtMozillaWidget_t *moz = (PtMozillaWidget_t *)widget;

	switch( argt->type )  {

		case Pt_ARG_MOZ_GET_URL:
			MozLoadURL(moz, (char *)(argt->value));
			break;

		case Pt_ARG_MOZ_NAVIGATE_PAGE:
			if (moz->MyBrowser->WebNavigation)
			{
				if (argt->value == WWW_DIRECTION_FWD)
					moz->MyBrowser->WebNavigation->GoForward();
				else
					moz->MyBrowser->WebNavigation->GoBack();
			}
			break;

		case Pt_ARG_MOZ_STOP:
			if (moz->MyBrowser->WebNavigation)
        moz->MyBrowser->WebNavigation->Stop(nsIWebNavigation::STOP_ALL);
			break;

		case Pt_ARG_MOZ_RELOAD:
			if (moz->MyBrowser->WebNavigation)
    			moz->MyBrowser->WebNavigation->Reload(0);
			break;

		case Pt_ARG_MOZ_PRINT: {
			nsCOMPtr<nsIDOMWindow> window;
			moz->MyBrowser->WebBrowser->GetContentDOMWindow(getter_AddRefs(window));
			nsCOMPtr<nsIWebBrowserPrint> print( do_GetInterface( moz->MyBrowser->WebBrowser ) );
			
			
			WWWRequest *pPageInfo = ( WWWRequest * ) argt->value;
			PpPrintContext_t *pc = moz_construct_print_context( pPageInfo );

			nsresult rv;
			nsCOMPtr<nsIPrintOptions> printService =
			             do_GetService(kPrintOptionsCID, &rv);
			printService->SetEndPageRange( (PRInt32) pc ); /* use SetEndPageRange/GetEndPageRange to convey the print context */
			print->Print( window, printService, moz->MyBrowser->WebBrowserContainer );
		    }
		    break;

		case Pt_ARG_MOZ_OPTION:
			mozilla_set_pref( widget, (char*)argt->len, (char*)argt->value );
			break;

		case Pt_ARG_MOZ_ENCODING: {
 			moz->MyBrowser->mPrefs->SetUnicharPref(
 				"intl.charset.default",
 				NS_ConvertASCIItoUCS2((char*)argt->value).get()
 			);
			}
			break;

		case Pt_ARG_MOZ_COMMAND:

			switch ((int)(argt->value)) {
				case Pt_MOZ_COMMAND_CUT: {
					nsCOMPtr<nsIContentViewer> viewer;
					moz->MyBrowser->rootDocShell->GetContentViewer(getter_AddRefs(viewer));
					nsCOMPtr<nsIContentViewerEdit> edit(do_QueryInterface(viewer));
					edit->CutSelection();
					}
					break;
				case Pt_MOZ_COMMAND_COPY: {
					nsCOMPtr<nsIContentViewer> viewer;
					moz->MyBrowser->rootDocShell->GetContentViewer(getter_AddRefs(viewer));
					nsCOMPtr<nsIContentViewerEdit> edit(do_QueryInterface(viewer));
					edit->CopySelection();
					}
					break;
				case Pt_MOZ_COMMAND_PASTE: {
					nsCOMPtr<nsIContentViewer> viewer;
					moz->MyBrowser->rootDocShell->GetContentViewer(getter_AddRefs(viewer));
					nsCOMPtr<nsIContentViewerEdit> edit(do_QueryInterface(viewer));
					edit->Paste();
					}
					break;
				case Pt_MOZ_COMMAND_SELECTALL: {
					nsCOMPtr<nsIContentViewer> viewer;
					moz->MyBrowser->rootDocShell->GetContentViewer(getter_AddRefs(viewer));
					nsCOMPtr<nsIContentViewerEdit> edit(do_QueryInterface(viewer));
					edit->SelectAll();
					}
					break;
				case Pt_MOZ_COMMAND_CLEAR: {
					nsCOMPtr<nsIContentViewer> viewer;
					moz->MyBrowser->rootDocShell->GetContentViewer(getter_AddRefs(viewer));
					nsCOMPtr<nsIContentViewerEdit> edit(do_QueryInterface(viewer));
					edit->ClearSelection();
					}
					break;

				case Pt_MOZ_COMMAND_FIND: {
					PtWebCommand_t *wdata = ( PtWebCommand_t * ) argt->len;
					nsCOMPtr<nsIWebBrowserFind> finder( do_GetInterface( moz->MyBrowser->WebBrowser ) );
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
				moz->MyBrowser->WebBrowserContainer->OpenStream( moz->MyBrowser->WebBrowser, "file://", "text/html" );
				strcpy( moz->url, (char*)argt->value );
				break;

		case Pt_ARG_MOZ_UNKNOWN_RESP: {
				PtMozUnknownResp_t *data = ( PtMozUnknownResp_t * ) argt->value;
				switch( data->response ) {
					case WWW_RESPONSE_OK:
						if( moz->MyBrowser->app_launcher ) {
							if( moz->download_dest ) free( moz->download_dest );
							moz->download_dest = strdup( data->filename );
							moz->MyBrowser->app_launcher->SaveToDisk( NULL, PR_TRUE );
							}
						break;
					case WWW_RESPONSE_CANCEL: {
							moz->MyBrowser->app_launcher->Cancel( );
							moz->MyBrowser->app_launcher->CloseProgressWindow( );
							moz->MyBrowser->app_launcher = NULL;
							}
						break;
					}
				}
				break;

		case Pt_ARG_MOZ_DOWNLOAD: {
				moz->MyBrowser->WebBrowserContainer->mSkipOnState = 1; /* ignore nsIWebProgressListener's CWebBrowserContainer::OnStateChange() for a while */
				moz->MyBrowser->WebNavigation->LoadURI( NS_ConvertASCIItoUCS2( (char*) argt->value ).get(), nsIWebNavigation::LOAD_FLAGS_NONE);

				if( moz->download_dest ) free( moz->download_dest );
				moz->download_dest = strdup( (char*)argt->len );
				}
				break;

		case Pt_ARG_MOZ_WEB_DATA: {
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
							strcpy( cb.url, moz->url );
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
						strcpy( cb.url, moz->url );
						PtInvokeCallbackList( moz->web_data_req_cb, (PtWidget_t *)moz, &cbinfo);
						}
						break;
					case WWW_DATA_CLOSE:
						if( !moz->MyBrowser->WebBrowserContainer->IsStreaming( ) ) break;
						moz->MyBrowser->WebBrowserContainer->CloseStream( );
						break;
					}
				}
			break;
			}

	return;
	}


// get resources function
static int mozilla_get_info(PtWidget_t *widget, PtArg_t *argt)
{
	PtMozillaWidget_t *moz = (PtMozillaWidget_t *) widget;
	int retval;

	switch (argt->type)
	{
		case Pt_ARG_MOZ_NAVIGATE_PAGE:
			moz->MyBrowser->WebNavigation->GetCanGoBack(&retval);
			moz->navigate_flags = 0;
			if (retval)
				moz->navigate_flags |= ( 1 << WWW_DIRECTION_BACK );
			moz->MyBrowser->WebNavigation->GetCanGoForward(&retval);
			if (retval)
				moz->navigate_flags |= ( 1 << WWW_DIRECTION_FWD );
			*((int **)argt->value) = &moz->navigate_flags;
			break;

		case Pt_ARG_MOZ_OPTION:
			mozilla_get_pref( widget, (char*)argt->len, (char*) argt->value );
			break;

		case Pt_ARG_MOZ_GET_CONTEXT:
			if( moz->rightClickUrl ) strcpy( (char*) argt->value, moz->rightClickUrl );
			else *(char*) argt->value = 0;
			break;

		case Pt_ARG_MOZ_ENCODING: {
			PRUnichar *charset = nsnull;
			moz->MyBrowser->mPrefs->GetLocalizedUnicharPref( "intl.charset.default", &charset );

			strcpy( (char*)argt->value, NS_ConvertUCS2toUTF8(charset).get() );
			}
			break;

		case Pt_ARG_MOZ_GET_HISTORY: {
			WWWRequest *req = ( WWWRequest * ) argt->len;
			PtWebClientHistoryData_t *HistoryReplyBuf = (PtWebClientHistoryData_t *) argt->value;
			int i, j;
			PRInt32 total;
			moz->MyBrowser->mSessionHistory->GetCount( &total );

			for( i=total-2, j=0; i>=0 && j<req->History.num; i--, j++) {
				nsIHistoryEntry *entry;
				moz->MyBrowser->mSessionHistory->GetEntryAtIndex( i, PR_FALSE, &entry );
			
				PRUnichar *title;
				nsIURI *url;
				entry->GetTitle( &title );
				entry->GetURI( &url );

				nsString stitle( title );
			  strncpy( HistoryReplyBuf[j].title, ToNewCString(stitle), 127 );
			  HistoryReplyBuf[j].title[127] = '\0';

				char *urlspec;
				url->GetSpec( &urlspec );
			  strcpy( HistoryReplyBuf[j].url, urlspec );
			  }

			}
			break;
	}

	return (Pt_CONTINUE);
}

#define VOYAGER_TEXTSIZE0					14
#define VOYAGER_TEXTSIZE1					16
#define VOYAGER_TEXTSIZE2					18
#define VOYAGER_TEXTSIZE3					20
#define VOYAGER_TEXTSIZE4					24

static void mozilla_set_pref( PtWidget_t *widget, char *option, char *value ) {
	PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) widget;
	char buffer[1024];

	mozilla_get_pref( widget, option, buffer );
	if( buffer[0] && !strcmp( value, buffer ) ) {
		return; /* the option is already set */
		}

/* HTML Options */
	if( !strcmp( option, "A:visited color" ) ) {
		moz->MyBrowser->mPrefs->SetUnicharPref( "browser.visited_color", NS_ConvertASCIItoUCS2(value).get() );
		}
	else if( !strcmp( option, "A:link color" ) ) {
		moz->MyBrowser->mPrefs->SetUnicharPref( "browser.anchor_color", NS_ConvertASCIItoUCS2(value).get() );
		}

/* the mozserver already has A:link color == browser.anchor_color for this */
//	else if( !strcmp( option, "A:active color" ) ) {
//		moz->MyBrowser->mPrefs->SetUnicharPref( "browser.anchor_color", NS_ConvertASCIItoUCS2(value).get() );
//		}

	else if( !strcmp( option, "BODY color" ) ) {
		moz->MyBrowser->mPrefs->SetUnicharPref( "browser.display.foreground_color", NS_ConvertASCIItoUCS2(value).get() );
		}
	else if( !strcmp( option, "BODY background" ) ) {
		moz->MyBrowser->mPrefs->SetUnicharPref( "browser.display.background_color", NS_ConvertASCIItoUCS2(value).get() );
		}
	else if( !strcmp( option, "bIgnoreDocumentAttributes" ) )
		moz->MyBrowser->mPrefs->SetBoolPref( "browser.display.use_document_colors", stricmp( value, "TRUE" ) ? PR_FALSE : PR_TRUE );
	else if( !strcmp( option, "bUnderlineLinks" ) ) {
		moz->MyBrowser->mPrefs->SetBoolPref( "browser.underline_anchors", !stricmp( value, "TRUE" ) ? PR_TRUE : PR_FALSE );
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
		moz->MyBrowser->mPrefs->SetIntPref( "font.size.variable.x-western", n );
		}
	else if( !strcmp( option, "BODY font-family" ) ) {
		/* set the current font */
		char *font_default = NULL;
		char preference[256];

		moz->MyBrowser->mPrefs->CopyCharPref( "font.default", &font_default );
		if( !font_default ) font_default = "serif";

		sprintf( preference, "font.name.%s.x-western", font_default );
		moz->MyBrowser->mPrefs->SetCharPref( preference, value );
		}
	else if( !strcmp( option, "PRE font-family" ) || !strcmp( option, "H* font-family" ) ) {
		/* do not set these - use the BODY font-family instead */
		}

/* SOCKS options */
	else if( !strcmp( option, "socks_server" ) )
		moz->MyBrowser->mPrefs->SetCharPref( "network.hosts.socks_server", value );
	else if( !strcmp( option, "socks_port" ) )
		moz->MyBrowser->mPrefs->SetCharPref( "network.hosts.socks_serverport", value );
	else if( !strcmp( option, "socks_user" ) ) 		; /* not used */
	else if( !strcmp( option, "socks_app" ) ) 		; /* not used */

/* HTTP options */
	else if( !strcmp( option, "http_proxy_host" ) )
		moz->MyBrowser->mPrefs->SetCharPref( "network.proxy.http", value );
	else if( !strcmp( option, "http_proxy_port" ) )
		moz->MyBrowser->mPrefs->SetCharPref( "network.proxy.http_port", value );
	else if( !strcmp( option, "proxy_overrides" ) )
		moz->MyBrowser->mPrefs->SetCharPref( "network.proxy.no_proxies_on", value );

/* FTP options */
	else if( !strcmp( option, "ftp_proxy_host" ) )
		moz->MyBrowser->mPrefs->SetCharPref( "network.proxy.ftp", value );
	else if( !strcmp( option, "ftp_proxy_port" ) )
		moz->MyBrowser->mPrefs->SetCharPref( "network.proxy.ftp_port", value );
	else if( !strcmp( option, "email_address" ) )		; /* not used */

/* Gopher options */
	else if( !strcmp( option, "gopher_proxy_host" ) )
		moz->MyBrowser->mPrefs->SetCharPref( "network.proxy.gopher", value );
	else if( !strcmp( option, "gopher_proxy_port" ) )
		moz->MyBrowser->mPrefs->SetCharPref( "network.proxy.gopher_port", value );

/* TCP/IP options */
	else if( !strcmp( option, "socket_timeout" ) )
		moz->MyBrowser->mPrefs->SetIntPref( "network.http.connect.timeout", atoi( value ) );
	else if( !strcmp( option, "max_connections" ) )
		moz->MyBrowser->mPrefs->SetIntPref( "network.http.max-connections", atoi( value ) );

/* Disk-cache options */
	else if( !strcmp( option, "main_cache_kb_size" ) )
		moz->MyBrowser->mPrefs->SetIntPref( "browser.cache.disk_cache_size", atoi( value ) );
	else if( !strcmp( option, "enable_disk_cache" ) )
		moz->MyBrowser->mPrefs->SetBoolPref( "browser.cache.disk.enable", !stricmp( value, "TRUE" ) ? PR_TRUE : PR_FALSE );
	else if( !strcmp( option, "dcache_verify_policy" ) ) {
		int n = atoi( value ), moz_value;
		if( n == 0 ) moz_value = 2; /* never */
		else if( n == 1 ) moz_value = 0; /* once */
		else moz_value = 1; /* always */
		moz->MyBrowser->mPrefs->SetIntPref( "browser.cache.check_doc_frequency", atoi( value ) );
		}
	else if( !strcmp( option, "main_cache_dir" ) ) 		; /* not used */
	else if( !strcmp( option, "main_index_file" ) ) 		; /* not used */
	else if( !strcmp( option, "clear_main_cache_on_exit" ) ) 		; /* not used */
	else if( !strcmp( option, "keep_index_file_updated" ) ) 		; /* not used */

/* Miscellaneous options */
	else if( !strcmp( option, "History_Expire" ) )
		moz->MyBrowser->mPrefs->SetIntPref( "browser.history_expire_days", atoi( value ) );
	else if( !strcmp( option, "Page_History_Length" ) )
		moz->MyBrowser->mPrefs->SetIntPref( "browser.sessionhistory.max_entries", atoi( value ) );



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
	else if( !strcmp( option, "disable_new_windows" ) )		; /* not used */
	else if( !strcmp( option, "disable_exception_dlg" ) )		; /* not used */
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
	}

static void mozilla_get_pref( PtWidget_t *widget, char *option, char *value ) {
	PtMozillaWidget_t *moz = ( PtMozillaWidget_t * ) widget;

	/* HTML Options */
	if( !strcmp( option, "A:link color" ) || !strcmp( option, "A:active color" ) ) {
		nsXPIDLCString colorStr;
		moz->MyBrowser->mPrefs->CopyCharPref( "browser.anchor_color", getter_Copies(colorStr) );
		strcpy( value, colorStr );
		}
	else if( !strcmp( option, "A:visited color" ) ) {
		nsXPIDLCString colorStr;
		moz->MyBrowser->mPrefs->CopyCharPref( "browser.visited_color", getter_Copies(colorStr) );
		strcpy( value, colorStr );
		}
	else if( !strcmp( option, "BODY color" ) ) {
		nsXPIDLCString colorStr;
		moz->MyBrowser->mPrefs->CopyCharPref( "browser.display.foreground_color", getter_Copies(colorStr) );
		strcpy( value, colorStr );
		}
	else if( !strcmp( option, "BODY background" ) ) {
		nsXPIDLCString colorStr;
		moz->MyBrowser->mPrefs->CopyCharPref( "browser.display.background_color", getter_Copies(colorStr) );
		strcpy( value, colorStr );
		}
	else if( !strcmp( option, "bIgnoreDocumentAttributes" ) ) {
		PRBool val;
		moz->MyBrowser->mPrefs->GetBoolPref( "browser.display.use_document_colors", &val );
		sprintf( value, "%s", val == PR_TRUE ? "FALSE" : "TRUE" );
		}
	else if( !strcmp( option, "bUnderlineLinks" ) ) {
		PRBool val;
		moz->MyBrowser->mPrefs->GetBoolPref( "browser.underline_anchors", &val );
		sprintf( value, "%s", val == PR_TRUE ? "TRUE" : "FALSE" );
		}
	else if( !strcmp( option, "iUserTextSize" ) ) {
		int n;
//		moz->MyBrowser->mPrefs->GetIntPref( "font.size.fixed.x-western", &n );
		moz->MyBrowser->mPrefs->GetIntPref( "font.size.variable.x-western", &n );
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

		moz->MyBrowser->mPrefs->CopyCharPref( "font.default", &font_default );
		if( !font_default ) font_default = "serif";

		sprintf( preference, "font.name.%s.x-western", font_default );
		moz->MyBrowser->mPrefs->CopyCharPref( preference, &font );
		strcpy( value, font );
		}

/* SOCKS options */
  else if( !strcmp( option, "socks_server" ) ) {
		char *s = NULL;
		moz->MyBrowser->mPrefs->CopyCharPref( "network.hosts.socks_server", &s );
		if( s ) strcpy( value, s );
		}
  else if( !strcmp( option, "socks_port" ) ) {
		char *s = NULL;
		moz->MyBrowser->mPrefs->CopyCharPref( "network.hosts.socks_serverport", &s );
		if( s ) strcpy( value, s );
		}

/* HTTP options */
  else if( !strcmp( option, "http_proxy_host" ) ) {
		char *s = NULL;
		moz->MyBrowser->mPrefs->CopyCharPref( "network.proxy.http", &s );
		if( s ) strcpy( value, s );
		}
  else if( !strcmp( option, "http_proxy_port" ) ) {
		char *s = NULL;
		moz->MyBrowser->mPrefs->CopyCharPref( "network.proxy.http_port", &s );
		if( s ) strcpy( value, s );
		}
  else if( !strcmp( option, "proxy_overrides" ) ) {
		char *s = NULL;
		moz->MyBrowser->mPrefs->CopyCharPref( "network.proxy.no_proxies_on", &s );
		if( s ) strcpy( value, s );
		}

/* FTP options */
  else if( !strcmp( option, "ftp_proxy_host" ) ) {
		char *s = NULL;
		moz->MyBrowser->mPrefs->CopyCharPref( "network.proxy.ftp", &s );
		if( s ) strcpy( value, s );
		}
  else if( !strcmp( option, "ftp_proxy_port" ) ) {
		char *s = NULL;
		moz->MyBrowser->mPrefs->CopyCharPref( "network.proxy.ftp_port", &s );
		if( s ) strcpy( value, s );
		}

/* Gopher options */
  else if( !strcmp( option, "gopher_proxy_host" ) ) {
		char *s = NULL;
		moz->MyBrowser->mPrefs->CopyCharPref( "network.proxy.gopher", &s );
		if( s ) strcpy( value, s );
		}
  else if( !strcmp( option, "gopher_proxy_port" ) ) {
    moz->MyBrowser->mPrefs->SetCharPref( "gopher_proxy_port", value );
		}

/* TCP/IP options */
  else if( !strcmp( option, "socket_timeout" ) ) {
		int n;
		moz->MyBrowser->mPrefs->GetIntPref( "network.http.connect.timeout", &n );
		sprintf( value, "%d", n );
		}
  else if( !strcmp( option, "max_connections" ) ) {
		int n;
		moz->MyBrowser->mPrefs->GetIntPref( "network.http.max-connections", &n );
		sprintf( value, "%d", n );
		}

/* Disk-cache options */
  else if( !strcmp( option, "main_cache_kb_size" ) ) {
		int n;
		moz->MyBrowser->mPrefs->GetIntPref( "browser.cache.disk_cache_size", &n );
		sprintf( value, "%d", n );
		}
  else if( !strcmp( option, "enable_disk_cache" ) ) {
		PRBool val;
		moz->MyBrowser->mPrefs->GetBoolPref( "browser.cache.disk.enable", &val );
		sprintf( value, "%s", val == PR_TRUE ? "TRUE" : "FALSE" );
		}
  else if( !strcmp( option, "dcache_verify_policy" ) ) {
		int n, voyager_value;
		moz->MyBrowser->mPrefs->GetIntPref( "browser.cache.check_doc_frequency", &n );
		if( n == 0 ) voyager_value = 1;
		else if( n == 1 ) voyager_value = 2;
		else if( n == 2 ) voyager_value = 0;
		sprintf( value, "%d", voyager_value );
    }

/* Miscellaneous options */
  else if( !strcmp( option, "History_Expire" ) ) {
		int n;
		moz->MyBrowser->mPrefs->GetIntPref( "browser.history_expire_days", &n );
		sprintf( value, "%d", n );
		}
  else if( !strcmp( option, "Page_History_Length" ) ) {
		int n;
		moz->MyBrowser->mPrefs->GetIntPref( "browser.sessionhistory.max_entries", &n );
		sprintf( value, "%d", n );
		}
	else *value = 0;

	}


static int InitProfiles( char *aProfileDir, char *aProfileName ) {

	if( !aProfileDir ) aProfileDir = "/tmp";

  // initialize profiles
  if( aProfileDir && aProfileName ) {
    nsresult rv;
    nsCOMPtr<nsILocalFile> profileDir;
    PRBool exists = PR_FALSE;
    PRBool isDir = PR_FALSE;
    profileDir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);

    rv = profileDir->InitWithPath(aProfileDir);
    if( NS_FAILED( rv ) ) return NS_ERROR_FAILURE;

    profileDir->Exists(&exists);
    profileDir->IsDirectory(&isDir);
    // if it exists and it isn't a directory then give up now.
    if( !exists ) {
      rv = profileDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
      if NS_FAILED( rv ) return NS_ERROR_FAILURE;
    	}
    else if (exists && !isDir) return NS_ERROR_FAILURE;

    // actually create the loc provider and initialize prefs.
    nsMPFileLocProvider *locProvider;
    // Set up the loc provider.  This has a really strange ownership
    // model.  When I initialize it it will register itself with the
    // directory service.  The directory service becomes the owning
    // reference.  So, when that service is shut down this object will
    // be destroyed.  It's not leaking here.
    locProvider = new nsMPFileLocProvider;
    rv = locProvider->Initialize(profileDir, aProfileName);

		// Load preferences service
		nsIPref *mPrefs;
		rv = nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref), (nsISupports **)&mPrefs);
		if( !NS_FAILED( rv ) ) {
    	mPrefs->ResetPrefs();
  	  mPrefs->ReadUserPrefs( nsnull );  //Reads from default_prefs.js
			}
		nsServiceManager::ReleaseService( kPrefCID, (nsISupports *)mPrefs );

		}
  return NS_OK;
	}


static int event_processor_callback(int fd, void *data, unsigned mode)
{
  nsIEventQueue *eventQueue = (nsIEventQueue*)data;
  if (eventQueue)
    eventQueue->ProcessPendingEvents();

  return Pt_CONTINUE;
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
	};

	static const PtClassRawCallback_t callback = { Ph_EV_INFO, mozilla_ev_info };

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
		{ Pt_SET_RAW_CALLBACKS, (long) &callback, 1 },
		{ Pt_SET_CHILD_GETTING_FOCUS_F, ( long ) child_getting_focus },
//		{ Pt_SET_CHILD_LOSING_FOCUS_F, ( long ) child_losing_focus },
//		{ Pt_SET_GOT_FOCUS_F, ( long ) got_focus },
//		{ Pt_SET_LOST_FOCUS_F, ( long ) lost_focus },
		{ Pt_SET_RESOURCES, (long) resources },
		{ Pt_SET_NUM_RESOURCES, sizeof( resources )/sizeof( resources[0] ) },
		{ Pt_SET_DESCRIPTION, (long) "PtMozilla" },
	};

	PtMozilla->wclass = PtCreateWidgetClass(PtContainer, 0, sizeof(args)/sizeof(args[0]), args);

	// initialize embedding
	NS_InitEmbedding(nsnull, nsnull);


#if 1 // don't need app shell this way
	// set up the thread event queue
	nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(kEventQueueServiceCID);
	if (!eventQService) exit (-1);

	nsCOMPtr< nsIEventQueue> eventQueue;
	eventQService->GetThreadEventQueue( NS_CURRENT_THREAD, getter_AddRefs(eventQueue));
	if (!eventQueue) 
		exit(-1);

	PtAppAddFdPri(NULL, eventQueue->GetEventQueueSelectFD(), (Pt_FD_READ | Pt_FD_NOPOLL | Pt_FD_DRAIN), event_processor_callback,eventQueue, getprio( 0 ) + 1 );
	printf("Event Queue added\n");
#else
	// create an app shell for event handling
	nsCOMPtr <nsIAppShell> gAppShell = do_CreateInstance(kAppShellCID);
	gAppShell->Create(0, nsnull);
	gAppShell->Spinup();
#endif

	InitProfiles( getenv( "HOME" ), ".mozilla" );
	Init_nsUnknownContentTypeHandler_Factory( );

	nsCOMPtr<nsIFactory> promptFactory;
	NS_NewPromptServiceFactory(getter_AddRefs(promptFactory));
	nsComponentManager::RegisterFactory(kPromptServiceCID,
                                    	"Prompt Service",
                                    	"@mozilla.org/embedcomp/prompt-service;1",
                                    	promptFactory,
                                    	PR_TRUE); // replace existing

	InitializeWindowCreator( );

	return (PtMozilla->wclass);
}
