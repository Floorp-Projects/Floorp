/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// RandomFrontEndCrap.cp


#include "fe_proto.h"
#include "RandomFrontEndCrap.h"
#include "CNSContext.h"
#include "CBrowserContext.h"
#include "nppg.h"
#include "libimg.h"
#include "libi18n.h"
#include "uprefd.h"
#include "mimages.h"
#include "earlmgr.h"
#include "UStdDialogs.h"
#include "macutil.h"
#include "CHTMLView.h"
#include "CHyperScroller.h" // for FE_QueryChrome
#include "resgui.h" // for 'neverAgain'
#include "ufilemgr.h"
#include "prefwutil.h" // for CFilePicker
#include "mime.h" // for FE_UsersOrganization proto
#include "msgcom.h" // for FE_GetTempFileFor proto
#include "StBlockingDialogHandler.h"

	// stuff added by deeje for the chromeless window hack
#include "CWindowMediator.h"
#include "CURLDispatcher.h"
#include "CBrowserWindow.h"
#ifdef MOZ_MAIL_NEWS
#include "CMessageWindow.h"
#include "CThreadWindow.h"
#endif

#include "CMochaHacks.h" // for managing dependent windows in javascript - 1997-02-22 mjc
#include "findw.h" // to set up find with CFindWindow statics -1997-02-27 mjc

#include "uapp.h"		/* for GetApplication - EA */
#include "VerReg.h" 	/* for Netcaster installed checks - EA*/
#include "xpgetstr.h"	/* for Netcaster error message - EA */
#include "uerrmgr.h"	/* for Netcaster error message - EA */
#include "libmocha.h"	/* Netcaster checks if mocha is enabled */
#include "java.h"	/* Netcaster checks if java is enabled. */

#include "uprefd.h"
#include "InternetConfig.h"
GDHandle DeviceForWindow(LWindow* inWindow);

#define WANT_ENUM_STRING_IDS 
#include "allxpstr.h" 		/* preferred mechanism for Netcaster error message - EA */
#undef WANT_ENUM_STRING_IDS 

NSPR_BEGIN_EXTERN_C
MWContext * FE_GetRDFContext();
NSPR_END_EXTERN_C

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- CONTEXT CALLBACKS ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

//	This is called when there's a chance that the state of the
//	Stop button (and corresponding menu item) has changed.
//	The FE should use XP_IsContextStoppable to determine the
//	new state.
 
void FE_UpdateStopState( MWContext*  )
{
// FIX ME!!! we should broadcast from the context
//	NETSCAPECONTEXT(context)->UpdateStopState();	
	// this had an empty NetscapeContext implementation.
	// CHyperWin had an implementation that adjusted the buttons
}

// FE_ScrollDocTo needed for javascript window.scrollTo() function - mjc 1/30/97
void FE_ScrollDocTo(MWContext *inContext, int /*inLocation*/, Int32 inX, Int32 inY)
{	
	CHTMLView* theHTMLView = ExtractHyperView(inContext);
	if (theHTMLView != nil) theHTMLView->SetDocPosition(0, inX, inY, true);
}

// FE_ScrollDocBy needed for javascript window.scrollBy() function - mjc 1/30/97
void FE_ScrollDocBy(MWContext *inContext, int /*inLocation*/, Int32 inX, Int32 inY)
{
	CHTMLView* theHTMLView = ExtractHyperView(inContext);
	if (theHTMLView != nil)
	{
		SPoint32 theScrollPosition;
		theHTMLView->GetScrollPosition(theScrollPosition);
		if (theHTMLView != nil) theHTMLView->SetDocPosition(0, theScrollPosition.h + inX, theScrollPosition.v + inY, true);
	}
}

PRBool FE_HandleLayerEvent(
	MWContext*				inContext,
	CL_Layer*				inLayer, 
	CL_Event*				inEvent)
{
	PRBool result = PR_FALSE;
	CBrowserContext* theBrowserContext = ExtractBrowserContext(inContext);
	if (theBrowserContext != NULL)
		result = theBrowserContext->HandleLayerEvent(inLayer, inEvent);
	return result;
}

PRBool FE_HandleEmbedEvent(
	MWContext*				inContext, 
	LO_EmbedStruct*			inEmbed, 
	CL_Event*				inEvent)
{
	CBrowserContext* theBrowserContext = ExtractBrowserContext(inContext);
	Assert_(theBrowserContext != NULL);
	return theBrowserContext->HandleEmbedEvent(inEmbed, inEvent);
}

MWContext* FE_MakeGridWindow(
	MWContext* 				inParentContext,
	void* 					inHistList,
	void* 					inHistEntry,
	int32					inX,
	int32 					inY,
	int32					inWidth,
	int32					inHeight,
	char* 					inURLString,
	char* 					inWindowTarget,
	int8					inScrollMode,
	NET_ReloadMethod 		inForceReload,
	Bool 					inNoEdge)
{
	CBrowserContext* theBrowserContext = ExtractBrowserContext(inParentContext);
	Assert_(theBrowserContext != NULL);
	return theBrowserContext->CreateGridContext(inHistList, inHistEntry, inX, inY, inWidth, inHeight, inURLString,
												inWindowTarget, inScrollMode, inForceReload, inNoEdge);
}

void* FE_FreeGridWindow(
	MWContext* 				inContext,
	XP_Bool 				inSaveHistory)
{
	CBrowserContext* theBrowserContext = ExtractBrowserContext(inContext);
	Assert_(theBrowserContext != NULL);
	return theBrowserContext->DisposeGridContext(inSaveHistory);
}

void FE_RestructureGridWindow(
	MWContext* 			inContext,
	int32 				inX,
	int32 				inY,
	int32 				inWidth,
	int32 				inHeight)
{
	CBrowserContext* theBrowserContext = ExtractBrowserContext(inContext);
	Assert_(theBrowserContext != NULL);
	theBrowserContext->RestructureGridContext(inX, inY, inWidth, inHeight);
}

void FE_GetFullWindowSize(
	MWContext* 			inContext,
	int32* 				outWidth,
	int32* 				outHeight)
{
	CBrowserContext* theBrowserContext = ExtractBrowserContext(inContext);
	Assert_(theBrowserContext != NULL);
	theBrowserContext->GetFullGridSize(*outWidth, *outHeight);
}

void FE_GetEdgeMinSize(MWContext */*context*/, int32 *size)
{
	*size = 5;
}


void FE_LoadGridCellFromHistory(
	MWContext* 			inContext,
	void* 				inHistEntry,
	NET_ReloadMethod 	inReload)
{
	CBrowserContext* theBrowserContext = ExtractBrowserContext(inContext);
	Assert_(theBrowserContext != NULL);
	theBrowserContext->ReloadGridFromHistory(inHistEntry, inReload);
}




void FE_SetRefreshURLTimer(
	MWContext*				context,
	uint32 					seconds,
	char*					refresh_url)
{
	if (ExtractHyperView(context))
		ExtractHyperView(context)->SetTimerURL(seconds, refresh_url);
}

void FE_ShowScrollBars(MWContext *context, XP_Bool show)
{
//Assert_(false);
	if (show)
		ExtractHyperView(context)->SetScrollMode(LO_SCROLL_YES);
	else
		ExtractHyperView(context)->SetScrollMode(LO_SCROLL_NO);
}

int FE_GetURL( MWContext* context, URL_Struct* url )
{
	CNSContext* theContext = ExtractNSContext(context);
	Assert_(theContext != NULL);
#if defined(MOZ_MAIL_NEWS) || defined (MOZ_LDAP)
	// 97-06-04 pkc -- check to see if we have correct context
	char *address = NET_URLStruct_Address(url);
	// 97-06-09 pkc -- for some reason, "search-libmsg:" isn't caught by
	// MSG_RequiresBrowserWindow
	if (strcasecomp(address, "search-libmsg:") &&
		strncasecomp(address, "ldap:", 5) &&
		MSG_RequiresBrowserWindow(address))
	{
		CBrowserContext* browserContext = ExtractBrowserContext(context);
		if (context->type != MWContextBrowser || !browserContext)
		{
			// We have a URL that requires a browser window, but we don't have a
			// browser context. Dispatch URL to new window
//			CURLDispatcher::GetURLDispatcher()->DispatchToView(NULL, url, FO_CACHE_AND_PRESENT, true);
			CURLDispatcher::DispatchURL(url, NULL, false, true);
			return 0;
		}
		else
		{
			theContext->SwitchLoadURL(url, FO_CACHE_AND_PRESENT);
			return 0;
		}
	}
	else
#endif // MOZ_MAIL_NEWS || MOZ_LDAP
	{
		if (theContext)
		{
			theContext->SwitchLoadURL(url, FO_CACHE_AND_PRESENT);
			return 0;
		}
	}
	return -1;
}

MWContext*
FE_MakeBlankWindow( MWContext* /*old_context*/, URL_Struct*, char* /*window_name*/ )
{
Assert_(false);
	return NULL;
}

void FE_SetWindowLoading(MWContext *, URL_Struct *,
        Net_GetUrlExitFunc **exit_func)
{
//	We should probably to the corresponding thing in the new world
//	NETSCAPECONTEXT(context)->StartSpinIcon(url, FALSE);
	// This is safe for now...
	*exit_func = EarlManager::DispatchFinishLoadURL;
}








MWContext* FE_MakeNewWindow(MWContext* old_context,
							URL_Struct* url,
							char* window_name,
							Chrome* chrome)
{
	
		// I have no idea what I'm doing, so bear with me
		// 1997-01-07 deeje
	
	MWContext*		theContext = nil;
	
	try
	{
		ResIDT					whichLayout = CBrowserWindow::res_ID;
		CBrowserContext*		theBrowserContext = nil;
		MWContext*				theTargetContext = XP_FindNamedContextInList(old_context, window_name);
		if (theTargetContext != nil)
		{
			theBrowserContext = ExtractBrowserContext(theTargetContext);
		}
		
		Boolean isDocInfoWindow = IsDocInfoWindow(window_name);
		
		short docInfoWindowWidth = qd.screenBits.bounds.right - 100;
		const short maxDocInfoWindowWidth = 600;
		
		if (docInfoWindowWidth > maxDocInfoWindowWidth)
			docInfoWindowWidth = maxDocInfoWindowWidth;
		
		short docInfoWindowHeight = qd.screenBits.bounds.bottom - 200;
		const short maxDocInfoWindowHeight = 900;
		
		if (docInfoWindowHeight > maxDocInfoWindowHeight)
			docInfoWindowHeight = maxDocInfoWindowHeight;
		
		// mjc select layout based on chrome. Relevant fields are titlebar, topmost, and z-lock.
		if (chrome != nil)
		{
			const ResIDT floatingHelpWindowResID = 1015;
			
			if (chrome->type == MWContextDialog)
			{
				if (isDocInfoWindow)
				{
//					chrome->topmost = true;
					chrome->location_is_chrome = true;
					chrome->w_hint = docInfoWindowWidth;
					chrome->h_hint = docInfoWindowHeight;
//					whichLayout = floatingHelpWindowResID;
				}
//				else if (chrome->allow_resize && chrome->allow_close)
				if (chrome->allow_resize && chrome->allow_close)
				{
					whichLayout = CBrowserWindow::res_ID;			// DOH!  it can be a MWContextDialog and still have non-dialog window bits!
				}
				else
				{
					whichLayout = CBrowserWindow::dialog_res_ID;
				}
			}
			else if ( chrome->type == MWContextHTMLHelp )
				whichLayout = floatingHelpWindowResID;
			else if (chrome->hide_title_bar == true)
				whichLayout = CBrowserWindow::titlebarless_res_ID; // titlebar-less browser window
			else if (chrome->topmost == true && chrome->hide_title_bar == false) // removed z-lock == true test - 2/11/97 mjc
				whichLayout = CBrowserWindow::floating_res_ID;
			else if (chrome->allow_resize)
				whichLayout = CBrowserWindow::res_ID;
			else	// If not explicitly resizable make sure window is created wo Zoom widget (Bug #s 86787, 96213)
				whichLayout = CBrowserWindow::nozoom_res_ID;
		}

			// ¥¥¥ also, there are places in the code (ie CURLDispatcher) which check the res_ID of a window
			// to determine a course of action.  So far, most of the code assumes only two ID values,
			// which is obviously not true anymore.  This needs to be addressed as well.   deeje 1997-01-16
			
			// A browser window now has a family of res ids. URLs are dispatched to windows by
			// finding the window corresponding to the context passed in. The type of window searched
			// for (browser or editor), depends on the res id passed in. There is no mapping of res ids
			// to type, and there should be. This works as is because the DispatchToView defaults the
			// res id to CBrowserWindow::res_id, so all the variations map to the browser window type.
			// 1997-02-28 mjc
			
			// make the window
		CURLDispatcher::DispatchURL(url, theBrowserContext, false, true, whichLayout, false);
		
		CBrowserWindow* theWindow = CURLDispatcher::GetLastBrowserWindowCreated();
				
		if (theWindow != nil)
		{
			theBrowserContext = (CBrowserContext*)theWindow->GetWindowContext();
			theContext = *theBrowserContext;
			if ((theContext != nil) && (window_name != nil))
			{
				theContext->name = XP_STRDUP(window_name);
				// 97-05-08 pkc -- set restricted target flag for "special" browser windows,
				// e.g. view source and doc info windows
				if (IsSpecialBrowserWindow(window_name))
					theBrowserContext->SetRestrictedTarget(true);
			}
			if (chrome != nil) 
			{
				theWindow->SetCanSaveStatus(false);
				if (chrome->dependent)
					CMochaHacks::AddDependent(old_context, theContext);
				if (chrome->hide_title_bar) theWindow->ClearAttribute(windAttr_TitleBar); // work around Constructor bug which sets this by default for non-standard windows
				if (chrome->restricted_target) theBrowserContext->SetRestrictedTarget(true);
				theWindow->SetChromeInfo(chrome, false, true);
			}
			if (isDocInfoWindow)
			{
				Point globalTopLeft =
				{
					qd.screenBits.bounds.bottom - docInfoWindowHeight - 50,
					qd.screenBits.bounds.right - docInfoWindowWidth - 50
				};
				
				CSaveWindowStatus::MoveWindowTo(theWindow, globalTopLeft);
			}
			/* jpm changes: get Shift_JIS & MacRoman 8bit to display correctly in security dialogs */
			if ((chrome != nil) && (theContext != nil))
			{
				if (chrome->type == MWContextDialog)
				{
					CNSContext* theNSContext = ExtractNSContext(theContext);
					Assert_(theNSContext != NULL);
					if (isDocInfoWindow)
					{
						/* Doc Info gets the user-selectable default encoding */
						theNSContext->SetDefaultCSID(INTL_DefaultWinCharSetID(NULL));
					} else {
						if (!IsViewSourceWindow(window_name))
						{
							/* all other HTML dialogs get the localised clients default encoding */
							theNSContext->SetDefaultCSID(INTL_CharSetNameToID(INTL_ResourceCharSet()));
						}
					}
					/* what about View Source? It inherits the encoding of the page being viewed */
				}
			}
			/* jpm end changes */
			theWindow->Show();
		}
	}
	catch(...)
	{
	}
	
	return theContext;
}



void FE_UpdateChrome(MWContext *inContext, Chrome *inChrome)
{
	CBrowserWindow* theWindow = CBrowserWindow::WindowForContext(ExtractBrowserContext(inContext));
	
	if (theWindow != nil)
	{
		theWindow->SetChromeInfo(inChrome);
	}
}

void FE_QueryChrome(MWContext *inContext, Chrome *inChrome)
{
	CBrowserWindow* theWindow = CBrowserWindow::WindowForContext(ExtractBrowserContext(inContext));

	if (theWindow != nil)
	{
		theWindow->GetChromeInfo(inChrome);
	}
	/* special case for inner width and height which are frame inner width and height if we
	 * are handed a frame, and window inner width and height if we are handed a window.
	 */
	if (inContext->grid_parent != NULL)
	{
	 	CHTMLView* theView = ExtractHyperView(inContext);
	 	if (theView)
	 	{
			CHyperScroller* theScroller = dynamic_cast<CHyperScroller*>(theView->GetSuperView());
			SDimension16	theInnerSize;
			if (theScroller) theScroller->GetFrameSize(theInnerSize);
			else theView->GetFrameSize(theInnerSize);
			inChrome->w_hint					= theInnerSize.width;
			inChrome->h_hint					= theInnerSize.height;
	 	}
	}
	 	
}






void FE_DestroyWindow( MWContext* context )
{
	LView* view = ExtractHyperView(context);
	if (view)
	{
		while( view != NULL )
		{
			view = view->GetSuperView();
			if (view)
			{
				LWindow* window = dynamic_cast<LWindow*>(view);
				if (window)
				{
					window->ObeyCommand(cmd_Close, NULL);
					break;
				}
			}
		}
		CMochaHacks::ClearSelectionForContext( context );
	}
}

void FE_Message(MWContext* context, const char* msg)
{
	if (context && context->type == MWContextBiff)
		return;
	
	CStr255 pmessage( msg );
	StripDoubleCRs( pmessage );

	LCommander* inSuper = LCommander::GetTopCommander();
	StBlockingDialogHandler theHandler( WIND_MessageDialog, inSuper);
	LWindow* theDialog = theHandler.GetDialog();

	// set the message
	LCaption* theCaption = (LCaption*)theDialog->FindPaneByID( PaneID_AlertCaption );
	Assert_( theCaption != NULL );
	theCaption->SetDescriptor( pmessage );

	if (UStdDialogs::TryToInteract())
	{
		theDialog->Show();
		MessageT theMessage = msg_Nothing;
		while(theMessage == msg_Nothing)
			theMessage = theHandler.DoDialog();
	}
}

void FE_Alert( MWContext* context, const char* msg )
{
	if ( context )
	{
		if (context->type == MWContextBiff)
			return;
		CNSContext* theContext = ExtractNSContext(context);
		Assert_(theContext != NULL);
		if (theContext)
			theContext->Alert(msg);
	}
	else
	{
		CStr255 	pmessage( msg );
		StripDoubleCRs( pmessage );
		UStdDialogs::Alert(pmessage, eAlertTypeCaution);
	}
}

char* FE_URLToLocalName( char *inURL )
{
    if( inURL == NULL )
        return NULL;

	char *  url = XP_STRDUP( inURL ); // Get our own copy
	if ( url == NULL )
		return NULL;
	
	// chop off everything after the first '?'
	char *end = XP_STRCHR(url, '?');
	if ( end )
		*end = '\0';
	
	// chop off everything after the first '#'
	end = XP_STRCHR( url, '#' );
	if ( end )
		*end = '\0';
	
	// Keep only what is to the right of the last slash
	char *front = XP_STRRCHR(url, '/');
	if ( front )
		strcpy( url, front + 1 );		// (or the whole string if there is no slash)
	
	if ( XP_STRLEN( url ) == 0 )
	{									// Is this really neccesary?
		XP_FREE(url);
		url = NULL;
	}
		
	return( url );
}


#define SECURITY_ALERT_RESID		1999
Bool FE_SecurityDialog( MWContext* /*context*/, int message, XP_Bool *retpref )
{
	short		result;
	
	result = CautionAlert( SECURITY_ALERT_RESID + message, NULL );
	
	if ( ( result == neverAgain ) && ( retpref != NULL ) )
	{
		*retpref = FALSE;
		result = ok;
	}
	else if ( message == SD_INSECURE_POST_FROM_SECURE_DOC )
	{
		if ( result == 1 )
			result = cancel;
		else if ( result == 2 )
			result = ok;
	}
	
	if ( result == cancel )
		return FALSE;
	else
		return TRUE;
}	
	
int32 FE_GetContextID( MWContext* window_id )
{
	return (int) window_id;
}

const char* FE_UsersMailAddress()
{
	return CPrefs::GetCharPtr( CPrefs::UserEmail );
}

const char* FE_UsersFullName()
{
	return CPrefs::GetCharPtr( CPrefs::UserName );
}

const char* FE_UsersSignature(void)
{
	static char*	signature = NULL;
	XP_File			file;
	
	if (!CPrefs::GetBoolean(CPrefs::UseSigFile))
		return NULL;

	if ( signature )
		XP_FREE( signature );
	if ( CPrefs::GetBoolean( CPrefs::UseInternetConfig ) )
	{
		Str255 sigString;
		memset( sigString, '\0', 256 );
		CInternetConfigInterface::GetInternetConfigString( kICSignature, sigString);
		signature = XP_STRDUP( (char*) sigString );
	}
	else
	{	
		file = XP_FileOpen( CStr255::sEmptyString, xpSignature, XP_FILE_READ );
		
		if ( file )
		{	
		    char		buf[ 1024 ];
		    char*		s = buf;
		    
		    int left = sizeof( buf ) - 2;
		    int size;
		    *s = 0;

		    while ( ((size = XP_FileRead( s, left, file ) )!=0) && left > 0 )
		    {
		        left -= size;
		        s += size;
			}

		    *s = 0;

		    /* take off all trailing whitespace */
		    s--;
		    while ( s >= buf && isspace (*s) )
		    	*s-- = 0;
		    /* terminate with a single newline. */
		    s++;
		    *s++ = '\r';
		    *s++ = 0;
		    XP_FileClose( file );
		    if ( !strcmp( buf, "\r" ) )
		    	signature = NULL;
		    else
			    signature = strdup( buf );
		}
		else
			signature = NULL;
	}		
	return signature;
}

void	FE_FileType(char * path, 
					Bool * useDefault, 
					char ** fileType, 
					char ** encoding)
{
	FSSpec spec;
	if ((path == NULL) || (fileType == NULL) || (encoding == NULL))
		return;

	*useDefault = TRUE;
	*fileType = NULL;
	*encoding = NULL;

	char *pathPart = NET_ParseURL( path, GET_PATH_PART);
	if (pathPart == NULL)
		return;

	OSErr err = CFileMgr::FSSpecFromLocalUnixPath(pathPart, &spec);	// Skip file://
	XP_FREE(pathPart);

	CMimeMapper * mapper = CPrefs::sMimeTypes.FindMimeType(spec);
	if (mapper != NULL)
	{
		*useDefault = FALSE;
		*fileType = XP_STRDUP(mapper->GetMimeName());
	}
	else
	{
		FInfo		fndrInfo;
		OSErr err = FSpGetFInfo( &spec, &fndrInfo );
		if ( (err != noErr) || (fndrInfo.fdType == 'TEXT') )
			*fileType = XP_STRDUP(APPLICATION_OCTET_STREAM);
		else
		{
			// Time to call IC to see if it knows anything
			ICMapEntry ICMapper;
			
			ICError  error = 0;
			CStr255 fileName( spec.name );
			error = CInternetConfigInterface::GetInternetConfigFileMapping(
					fndrInfo.fdType, fndrInfo.fdCreator,  fileName ,  &ICMapper );	
			if( error != icPrefNotFoundErr && StrLength(ICMapper.MIME_type) )
			{
				*useDefault = FALSE;
				CStr255 mimeName( ICMapper.MIME_type );
				*fileType = XP_STRDUP( mimeName );
			}
			else
			{
				// That failed try using the creator type		
				mapper = CPrefs::sMimeTypes.FindCreator(fndrInfo.fdCreator);
				if( mapper)
				{
					*useDefault = FALSE;
					*fileType = XP_STRDUP(mapper->GetMimeName());
				}
				else
				{
					// don't have a mime mapper
					*fileType = XP_STRDUP(APPLICATION_OCTET_STREAM);
				}
			}
		}
	}
}

const char*
FE_UsersOrganization( void )
{
	return CPrefs::GetCharPtr( CPrefs::Organization );
}

char* FE_GetTempFileFor( MWContext* /*cx*/, const char* filename, 
						XP_FileType ftype, XP_FileType* rettype )
{
	*rettype = ftype; // Required, if the stuff is to work!
	char * s = WH_TempName(ftype, filename);
	return s;
}

/* The semantics of this routine are badly broken */
/* We'll mimic Window's implementation for sanity */
int FE_PromptForFileName(	MWContext* context,
							const char* prompt_string,
							const char* default_path,
							XP_Bool file_must_exist_p,
							XP_Bool directories_allowed_p,
							ReadFileNameCallbackFunction fn,	// This function will free the file name
							void* closure )
{
	StandardFileReply		reply;
    XP_MEMSET(&reply, 0, sizeof(StandardFileReply));
	OSErr					err = -1;
	CFilePicker::PickEnum	pickType;
	
	if ( file_must_exist_p )
	{
		if ( directories_allowed_p )
			pickType = CFilePicker::Folders;
		else
			pickType = CFilePicker::AnyFile;
		
		if ( default_path )
			err = CFileMgr::FSSpecFromLocalUnixPath( default_path, &reply.sfFile );
			
		if ( CFilePicker::DoCustomGetFile( reply, pickType, err == noErr ) )
			goto copy;
		else
			return -1;
	}
	else
	{
		if ( !directories_allowed_p )
		{
			CStr255					prompt;
			CStr31					filename;
		
			if ( default_path )
			{
				filename = CFileMgr::FileNameFromURL( default_path );
				err = CFileMgr::FSSpecFromLocalUnixPath( default_path, &reply.sfFile );
				LString::CopyPStr( filename, reply.sfFile.name, 31 );
			}
			
			prompt = prompt_string;
			if ( CFilePicker::DoCustomPutFile( reply, prompt, err == noErr ) )
				goto copy;
			else
				return -1;
		}
		else	/* Directories , they are asking for a new folder, we give 'em existing for window's compatibility */
		{
			pickType = CFilePicker::Folders;
			if ( default_path )
				err = CFileMgr::FSSpecFromLocalUnixPath( default_path, &reply.sfFile );
				
			if ( CFilePicker::DoCustomGetFile( reply, pickType, err == noErr ) )
				goto copy;
			else
				return -1;
		}
	}

copy:
	FSSpec					smReply;
	char*					path;
	
	smReply = reply.sfFile;
	path = CFileMgr::EncodedPathNameFromFSSpec( smReply, TRUE );
	if ( fn )
		(*fn) ( context, path, closure );
	return noErr;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- JAVASCRIPT ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void FE_BackCommand (MWContext *inContext)
{
	CBrowserContext* theContext = ExtractBrowserContext(inContext);
	CBrowserWindow* theWindow = CBrowserWindow::WindowForContext(theContext);
	
	if (theWindow != nil && theContext->CanGoBack())
		theWindow->ObeyCommand(cmd_GoBack);
}

void FE_ForwardCommand (MWContext *inContext)
{
	CBrowserContext* theContext = ExtractBrowserContext(inContext);	
	CBrowserWindow* theWindow = CBrowserWindow::WindowForContext(theContext);
	
	if (theWindow != nil && theContext->CanGoForward())
		theWindow->ObeyCommand(cmd_GoForward);
}

void FE_HomeCommand (MWContext *inContext)
{
	CBrowserWindow* theWindow = CBrowserWindow::WindowForContext(ExtractBrowserContext(inContext));
	
	if (theWindow != nil)
	{
		theWindow->ObeyCommand(cmd_Home); // we can always go home
	}
}

void FE_PrintCommand (MWContext *inContext)
{
	CHTMLView* theHTMLView = ExtractHyperView(inContext);
	
	if (theHTMLView != nil)
	{
		Boolean				outEnabled;
		Boolean				outUsesMark;
		Char16				outMark;
		Str255				outName;
		// can we print?
		theHTMLView->FindCommandStatus(cmd_Print, outEnabled, outUsesMark, outMark, outName);
		if (outEnabled)
			theHTMLView->ObeyCommand(cmd_Print, NULL);
	}
}

// Searches for a string in the html view.
// ALERT: inWrap disabled because it is always false in backend - 1997-03-06 mjc
XP_Bool FE_FindCommand(
	MWContext *inContext, char* inSearchString,
	XP_Bool inCaseSensitive, XP_Bool inBackward, XP_Bool /*inWrap*/)
{
	CHTMLView* theView = ExtractHyperView(inContext);
	if (theView != nil)
	{
		Boolean				outEnabled;
		Boolean				outUsesMark;
		Char16				outMark;
		Str255				outName;
		// can we find?
		theView->FindCommandStatus(cmd_Find, outEnabled, outUsesMark, outMark, outName);
		if (outEnabled)
		{
			if (inSearchString != NULL)
			{
				if (strlen(inSearchString) == 0) return FALSE;
				// use CFindWindow statics to setup and do the find without creating a window
				// save and restore settings
				Boolean saveCaseSensitive = CFindWindow::sCaseless;
				Boolean saveBackward = CFindWindow::sBackward;
				//Boolean saveWrap = CFindWindow::sWrap;
				CFindWindow::sLastSearch = inSearchString;
				CFindWindow::sCaseless = !inCaseSensitive;
				CFindWindow::sBackward = inBackward;
				//CFindWindow::sWrap = inWrap;
				Boolean result = theView->DoFind();
				CFindWindow::sCaseless = saveCaseSensitive;
				CFindWindow::sBackward = saveBackward;
				//CFindWindow::sWrap = saveWrap;
				return result;
			}
			else 
			{
				CFindWindow::DoFind(theView); // create a find window
				return FALSE;
			}
		}
	}
	return FALSE;
}

// Returns in global coordinates the offset of the html view from the screen origin.
// The vertical component of the screen origin for javascript on the mac is the
// bottom of the menubar.
void FE_GetWindowOffset (MWContext *inContext, int32 *sx, int32 *sy)
{
	CHTMLView* theView = ExtractHyperView(inContext);
	if (theView != nil)
	{
		Point htmlOrigin = { 0, 0};
		theView->LocalToPortPoint(htmlOrigin);
		theView->PortToGlobalPoint(htmlOrigin);
		htmlOrigin.v -= ::GetMBarHeight();
		*sx = htmlOrigin.h;
		*sy = htmlOrigin.v;
	}
}

// Returns the device which contains the largest portion of
// the window's rectangle, or the main device if inWindow is null.
GDHandle DeviceForWindow(LWindow* inWindow)
{
	GDHandle	dominantDevice = nil;
	
	if (inWindow != nil)
	{
		Rect			windowFrame;
		
		inWindow->CalcPortFrameRect( windowFrame );
		windowFrame = PortToGlobalRect( inWindow, windowFrame );

		dominantDevice = UWindows::FindDominantDevice(windowFrame);
	}
	else dominantDevice = ::GetMainDevice();
	
	return ((dominantDevice == nil) ? ::GetMainDevice() : dominantDevice);
}

// Returns the screen size for the screen which contains the largest
// portion of the rectangle. If the context does not correspond to a browser window,
// returns the values for the main screen.
void FE_GetScreenSize (MWContext *inContext, int32 *sx, int32 *sy)
{
	GDHandle device;
	CBrowserWindow* theWindow = CBrowserWindow::WindowForContext(ExtractBrowserContext(inContext));
	
	device = DeviceForWindow(theWindow);

	*sx = (**device).gdRect.right;
	*sy = (**device).gdRect.bottom;
}

// Returns the top, left, width, and height of the available screen area, that is,
// on the Mac, the screen area minus the menubar. The screen which contains the
// largest portion of the window's rectangle is chosen, or the main screen if the
// context does not correspond to a browser window.
void FE_GetAvailScreenRect (MWContext *inContext, int32 *sx, int32 *sy, 
							int32 *left, int32 *top)
{
	GDHandle device;
	CBrowserWindow* theWindow = CBrowserWindow::WindowForContext(ExtractBrowserContext(inContext));
	
	device = DeviceForWindow(theWindow);

	*sx = (**device).gdRect.right;
	*sy = (**device).gdRect.bottom - ::GetMBarHeight();
	// left and top represent the origin in terms of window positioning so will always be 0, 0 on mac
	*left = 0; /* (**device).gdRect.left; */
	*top = 0; /* (**device).gdRect.top + ::GetMBarHeight();	*/
}

// Returns the pixel and color depth for the screen which contains the largest
// portion of the rectangle. If the context does not correspond to a browser window,
// returns the values for the main screen.
void FE_GetPixelAndColorDepth (MWContext *inContext, int32 *pixelDepth, 
				      int32 *colorDepth)
{
	GDHandle device;
	CBrowserWindow* theWindow = CBrowserWindow::WindowForContext(ExtractBrowserContext(inContext));
	
	device = DeviceForWindow(theWindow);
	
	*pixelDepth = (**(**device).gdPMap).pixelSize;
	*colorDepth = (**(**device).gdPMap).pixelSize; // ALERT: implement me!
}


// This function returns a context suitable for throwing up a dialog
// at init time. This dialog must be an FE_PromptPassword. If other dialogs
// are needed, we need to check all the FE's to make sure that their returned
// context can handle them. (rjr).
MWContext *FE_GetInitContext(void) {
	static CNSContext *rootContext = NULL;

	if (!rootContext) {
		rootContext = new CNSContext(MWContextDialog);
		if (rootContext) {
			// don't leave this context lieing around the XP list.
			XP_RemoveContextFromList((MWContext *)*rootContext);
		}
	}
	if (!rootContext) return NULL;
	return (MWContext *)*rootContext;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- PREFS CRAP ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// This class only works for now with the old prefs (CPrefs::SubscribeToPrefChanges /
// CPrefs::UnsubscribeToPrefChanges). It can be extended for the new style prefs
// (PREF_RegisterCallback / PREF_UnregisterCallback)

class PrefsChangeListener : public LListener
{
public:
						PrefsChangeListener(CPrefs::PrefEnum inID);
	virtual 			~PrefsChangeListener();
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam);
	virtual Boolean		PrefHasChanged();
protected:
	CPrefs::PrefEnum	mID;
	Boolean				mPrefHasChanged;
};

PrefsChangeListener::PrefsChangeListener(CPrefs::PrefEnum inID)
{
	mID = inID;
	mPrefHasChanged = true;		// default to true: must load pref the first time
	CPrefs::SubscribeToPrefChanges(this);
}

PrefsChangeListener::~PrefsChangeListener()
{
	CPrefs::UnsubscribeToPrefChanges(this);
}

void PrefsChangeListener::ListenToMessage(MessageT inMessage, void *ioParam)
{
	if (inMessage == CPrefs::msg_PrefsChanged)
	{
		if (*(CPrefs::PrefEnum *)ioParam == mID)
			mPrefHasChanged = true;
	}
}

Boolean PrefsChangeListener::PrefHasChanged()
{
	Boolean	value = mPrefHasChanged;
	mPrefHasChanged = false;
	return(value);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- INTERNATIONAL CALLBACKS ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void INTL_Relayout(MWContext* inContext)
{
	CBrowserContext* theBrowserContext = ExtractBrowserContext(inContext);
	
	// this can be called when quoting HTML on a plain text email; then we
	// have a CPlainTextConverterContext, not a CBrowserContext
	if (theBrowserContext != NULL)
		theBrowserContext->Repaginate();
}

static Int16 DefaultDocCharSetIDFromContext(MWContext* inContext);
static Int16 DefaultDocCharSetIDFromPref();
Int16 DefaultDocCharSetIDFromFrontWindow();

static Int16 DefaultDocCharSetIDFromContext(MWContext* inContext)
{
	Assert_(inContext != NULL);
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->GetDefaultCSID();
}

static PrefsChangeListener *	gCharSetIDPrefsListener = nil;

static Int16 DefaultDocCharSetIDFromPref()
{	
	static Int16 prefDefaultID;

	if (gCharSetIDPrefsListener == nil)
		gCharSetIDPrefsListener = new PrefsChangeListener(CPrefs::DefaultCharSetID);

	if (gCharSetIDPrefsListener->PrefHasChanged())
	{
		prefDefaultID = CPrefs::GetLong(CPrefs::DefaultCharSetID);
		if(0 == prefDefaultID)	// to fix John McMullen's old preference problem (bug 63052)
			prefDefaultID = CS_LATIN1;
	}
  	if (INTL_CanAutoSelect(prefDefaultID))
		prefDefaultID |= CS_AUTO;
	return prefDefaultID;
}

Int16 DefaultDocCharSetIDFromFrontWindow()
{
	CWindowIterator iter(WindowType_Any);
	CMediatedWindow* window;
	Int16 csid = 0;
	for (iter.Next(window); window; iter.Next(window))
	{	
		csid = window->DefaultCSIDForNewWindow();
		if(0 != csid)
			return csid;
	}
	return 0;
}

int16 INTL_DefaultDocCharSetID(MWContext* inContext)
{
	if (inContext != NULL)
		return DefaultDocCharSetIDFromContext(inContext);
	return DefaultDocCharSetIDFromPref();
}



// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- IMAGE LIB STUFF---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


void FE_ShiftImage(
	MWContext*			,
	LO_ImageStruct*		)
{
}
























// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- HELP STUFF ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

MWContext *FE_GetNetHelpContext()
{
	MWContext			*pActiveContext = NULL;
	CNSContext			*pWindowContext = NULL;
	CWindowIterator 	*iter;
	CMediatedWindow		*window;
	
	iter = new CWindowIterator(WindowType_Browser);
	iter->Next(window);

	if (window) {
		CBrowserWindow	*result;
		
		result = dynamic_cast<CBrowserWindow*>(window);
		pWindowContext = result->GetWindowContext();
	} else {
		delete iter;
		
		iter = new CWindowIterator(WindowType_MailNews);
		iter->Next(window);

#ifdef MOZ_MAIL_NEWS
		if (window) {
			CMailNewsWindow	*result;
			
			result = dynamic_cast<CMailNewsWindow*>(window);
			pWindowContext = result->GetWindowContext();
		}
#endif // MOZ_MAIL_NEWS
	}
	
	if (pWindowContext) {
		pActiveContext = (MWContext *) pWindowContext;
	}
	
	if (!pActiveContext) {
	
		/* Borrow a cheat from the AppleEvents code that did this.  The comment there was:
		   "Will using the bookmark context as a dummy work?"
		*/
		DebugStr("\pNot Implemented");
//¥¥¥		CBookmarksContext::Initialize();
//¥¥¥		pActiveContext = (MWContext *) ((CNSContext *) CBookmarksContext::GetInstance());
	}
	
	return pActiveContext;
}


//
// ShowHelp
//
// Displays the NetHelp window, given the appropriate help topic.
//

void
ShowHelp ( const char* inHelpTopic )
{
	const char	* const netHelpPrefix = "nethelp:netscape/";
	char		*fullURL = (char *)XP_ALLOC(strlen(inHelpTopic) +
											strlen(netHelpPrefix) + 1);
	if (fullURL)
	{
		strcpy(fullURL, netHelpPrefix);
		strcat(fullURL, inHelpTopic);
		CFrontApp::DoGetURL(fullURL);
		XP_FREE(fullURL);
	}
} // DoHelp


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- NETCASTER STUFF ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

/* Check if Netcaster is in the registry, and if a specific component is there
 * (the user may have deleted the files)
 */
XP_Bool FE_IsNetcasterInstalled()
{
	XP_Bool bInstalled = ((VR_InRegistry("Netcaster") == REGERR_OK) && (VR_ValidateComponent("Netcaster/tab.htm") == REGERR_OK));
	VR_Close();
	return bInstalled;
}

/* Runs Netcaster by finding an html component and opening it in a 
 * new browser window, or bringing the window to the front if Netcaster
 * is already launched.
 */
void FE_RunNetcaster(MWContext *)
{
	CFrontApp	*theApp = CFrontApp::GetApplication();
	MWContext	*pNetcasterContext = NULL;
	char *contextName = "Netcaster_SelectorTab";
	
	// The Mac function ignores the pContext parameter

	if (theApp != NULL) {
		pNetcasterContext = theApp->GetNetcasterContext();
		if (pNetcasterContext != NULL) {
			// we've already launched.  Just bring it to front.
			FE_RaiseWindow(pNetcasterContext);
		} else {
			// we haven't launched yet.  Do that now.

		 	Chrome          netcasterChrome;
		 	URL_Struct*     URL_s;
			REGERR			regErr;
			char			netcasterPath[1024];

			// get the native path to the component.
			regErr = VR_GetPath("Netcaster/tab.htm", 1024, netcasterPath);
			VR_Close();
			
			if (regErr == REGERR_OK) {
			
				FSSpec netcasterFile;
				char *netcasterURL;
				// translate the native path to a local file URL.
				CFileMgr::FSSpecFromPathname(netcasterPath, &netcasterFile);
				netcasterURL = CFileMgr::GetURLFromFileSpec(netcasterFile);
			
				if (netcasterURL != NULL)
				{
					// javascript and java must be enabled for Netcaster to run.
					if (!LM_GetMochaEnabled() || !LJ_GetJavaEnabled())
					{
						char	*errString = XP_GetString(XP_ALERT_NETCASTER_NO_JS);
						
						ErrorManager::PlainAlert(errString, "", "", "");
						XP_FREE(netcasterURL);
						return;
					}
				 	memset(&netcasterChrome, 0, sizeof(Chrome));

				 	netcasterChrome.w_hint = 22;
				 	netcasterChrome.h_hint = 59;
				 	netcasterChrome.l_hint = -300;
				 	netcasterChrome.t_hint = 0;
				 	netcasterChrome.topmost = TRUE;
				 	netcasterChrome.z_lock = TRUE;
				 	netcasterChrome.location_is_chrome = TRUE;
				 	netcasterChrome.disable_commands = TRUE;
				 	netcasterChrome.hide_title_bar = TRUE;
				 	netcasterChrome.restricted_target = TRUE;
				 	netcasterChrome.allow_close = TRUE;
				 
					URL_s = NET_CreateURLStruct(netcasterURL, NET_DONT_RELOAD);
				 
					pNetcasterContext = FE_MakeNewWindow(NULL, 
														 URL_s, 
				 										 contextName, 
				 										 &netcasterChrome);
				 										 
				 	theApp->SetNetcasterContext(pNetcasterContext);
				 	XP_FREE(netcasterURL);
			 	}
			 } else {
				char	*errString = XP_GetString(XP_ALERT_CANT_RUN_NETCASTER);
				
				ErrorManager::PlainAlert(errString, "", "", "");
			 }
		}
	} else {
		XP_ASSERT(0);
	}
}

MWContext * FE_IsNetcasterRunning(void)
{
	CFrontApp	*theApp = CFrontApp::GetApplication();
	
	if (theApp != NULL)
		return theApp->GetNetcasterContext();
	else return NULL;
}

MWContext * FE_GetRDFContext()
{
	try
	{
		if (!CFrontApp::sRDFContext.get())
		{
			CFrontApp::sRDFContext.reset(new CNSContext(MWContextRDFSlave));
		}
		return *CFrontApp::sRDFContext.get();
	}
	catch (...)
	{
		return NULL;
	}
}

// stub this for now
void FE_SetPasswordEnabled( MWContext*, PRBool )
{
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- SPURIOUS CRAP ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char reconnectHack[] = "internal-external-reconnect";
char mailNewsReconnect[] = "internal-external-reconnect:";

const char* cDocInfoWindowContextName = "%DocInfoWindow";
const char* cViewSourceWindowContextName1 = "%ViewSourceWindow";
const char* cViewSourceWindowContextName2 = "view-source";
const size_t cViewSourceWindowString1Length = 17;
const size_t cViewSourceWindowString2Length = 11;

Boolean IsMailToLink( const char* url )
{
	if ( !XP_STRNCMP( url, "mailto", 6 ) )
		return TRUE;
	return FALSE;
}

Boolean IsInternalImage( const char* url )
{
	if ( !XP_STRNCMP( url, reconnectHack, XP_STRLEN( reconnectHack ) ) )
		return TRUE;
	return FALSE;
}

Boolean IsMailNewsReconnect( const char* url )
{
	if ( !XP_STRNCMP( url, mailNewsReconnect, XP_STRLEN( mailNewsReconnect ) ) )
		return TRUE;
	return FALSE;
}

Boolean IsInternalTypeLink( const char* url )
{
	if ( !XP_STRNCMP( url, "internal-", 9 ) )
		return TRUE;
	return FALSE;
}

Boolean IsDocInfoWindow(const char* inName)
{
	if (inName)
		return (XP_STRCMP(inName, cDocInfoWindowContextName) == 0);
	else
		return false;
}

Boolean IsViewSourceWindow(const char* inName)
{
	Boolean result = false;
	if (inName && (XP_STRLEN(inName) >= cViewSourceWindowString2Length))
	{
		result = (XP_STRNCMP(inName,
							 cViewSourceWindowContextName1,
							 cViewSourceWindowString1Length) == 0);
		if (!result)
		{
			result = (XP_STRCASECMP(inName, cViewSourceWindowContextName2) == 0);
		}
	}
	return result;
}

Boolean IsSpecialBrowserWindow(const char* inName)
{
	Boolean result = false;
	if (inName)
	{
		if (IsDocInfoWindow(inName))
			result = true;
		if (IsViewSourceWindow(inName))
			result = true;
		if (XP_STRCMP(inName, XP_GetString(XP_SECURITY_ADVISOR_TITLE_STRING)) == 0)
			result = true;
	}
	return result;
}
