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

//	CBrowserContext.cp

#include "CBrowserContext.h"
#include "CNSContextCallbacks.h"
#include "CHTMLView.h"
#ifndef NSPR20
#include "CNetwork.h"
#endif
#include "UStdDialogs.h"
#include "UFormElementFactory.h"
#include "CAutoPtrXP.h"
#include "RandomFrontEndCrap.h" // for IsSpecialBrowserWindow
#include "CURLDispatcher.h"

#include "CFontReference.h"

#include <LString.h>

#include "earlmgr.h"
#include "uprefd.h"
#include "xp.h"
#include "xp_thrmo.h"
#include "shist.h"
#include "glhist.h"
#include "libimg.h"
#include "np.h"
#include "libmime.h"
#include "libi18n.h"
#include "mimages.h"
#include "ufilemgr.h"
#include "java.h"
#include "layers.h"
#include "intl_csi.h"
#include "uintl.h"

// FIX ME -- write a CopyAlloc like function that takes a CString
#include "macutil.h"
#include "CAutoPtrXP.h"

const ResIDT cJSDialogTitleStrID = 16010;
const Int16 cDialogTitleSeperatorStrIndex = 1;
const Int16 cDialogTitleStrIndex = 2;

// utility function to build window title for JavaScript dialogs
static void AppendJSStringToHost(LStr255& ioTitle, const char* inURL)
{
	LStr255 titleSeperatorStr(cJSDialogTitleStrID, cDialogTitleSeperatorStrIndex);
	LStr255 jsAppStr(cJSDialogTitleStrID, cDialogTitleStrIndex);

	if (inURL)
	{
		CAutoPtrXP<char> hostname(NET_ParseURL(inURL, GET_HOST_PART));
		char *host = hostname.get();
		if (host && *host)
		{
			ioTitle = host;
			ioTitle += titleSeperatorStr;
			ioTitle += jsAppStr;
		}
		else
		{
			ioTitle = jsAppStr;
		}
	}
	else
	{
		ioTitle = jsAppStr;
	}
	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- CONSTRUCTION / DESTRUCTION ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CBrowserContext::CBrowserContext()
:	CNSContext(MWContextBrowser)
,	mCompositor(nil)
,	mImageGroupContext(nil)
,	mImagesLoading(false)
,	mImagesLooping(false)
,	mImagesDelayed(false)
,	mMochaImagesLoading(false)
,	mMochaImagesLooping(false)
,	mMochaImagesDelayed(false)
,	mInNoMoreUsers(false)
,	mCloseCallback(nil)
,	mCloseCallbackArg(nil)
{
	mLoadImagesOverride = false;
	mDelayImages = 	CPrefs::GetBoolean( CPrefs::DelayImages );
	mIsRepaginating = false;
	mIsRepaginationPending = false;
	
// FIX ME!!! need to add unique identifier
//	fWindowID = sWindowID++;

//	XP_AddContextToList(&mContext);
	SHIST_InitSession(&mContext);
		
	//
	// Allocate a new image library context
	//
	CreateImageContext( &mContext );
	mImageGroupContext = mContext.img_cx;
	IL_AddGroupObserver(mImageGroupContext, ImageGroupObserver, &mContext);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CBrowserContext::CBrowserContext(MWContextType inType) 
:	CNSContext(inType)
,	mCompositor(nil)
,	mImageGroupContext(nil)
,	mImagesLoading(false)
,	mImagesLooping(false)
,	mImagesDelayed(false)
,	mMochaImagesLoading(false)
,	mMochaImagesLooping(false)
,	mMochaImagesDelayed(false)
,	mInNoMoreUsers(false)
,	mCloseCallback(nil)
,	mCloseCallbackArg(nil)
{
	mLoadImagesOverride = false;
	mDelayImages = 	CPrefs::GetBoolean( CPrefs::DelayImages );
	mIsRepaginating = false;
	mIsRepaginationPending = false;
	
// FIX ME!!! need to add unique identifier
//	fWindowID = sWindowID++;

//	XP_AddContextToList(&mContext);
	SHIST_InitSession(&mContext);
	
	//
	// Allocate a new image library context
	//
	CreateImageContext( &mContext );
	mImageGroupContext = mContext.img_cx;
	IL_AddGroupObserver(mImageGroupContext, ImageGroupObserver, &mContext);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// this is a shallow copy.  child contexts are not duplicated.
CBrowserContext::CBrowserContext(const CBrowserContext& inOriginal)
:	CNSContext(inOriginal)
,	mCompositor(nil)
,	mImageGroupContext(nil)
,	mImagesLoading(false)
,	mImagesLooping(false)
,	mImagesDelayed(false)
,	mMochaImagesLoading(false)
,	mMochaImagesLooping(false)
,	mMochaImagesDelayed(false)
,	mInNoMoreUsers(false)
,	mCloseCallback(nil)
,	mCloseCallbackArg(nil)
{
	mLoadImagesOverride = inOriginal.IsLoadImagesOverride();
	mDelayImages = 	inOriginal.IsImageLoadingDelayed();
	mIsRepaginating = inOriginal.IsRepaginating();
	mIsRepaginationPending = inOriginal.IsRepagintaitonPending();
	SetCompositor(inOriginal.mCompositor);

// FIX ME!!! need to make sure all things inited in the default ctor are done here

	//
	// Allocate a new image library context
	//
	CreateImageContext( &mContext );
	mImageGroupContext = mContext.img_cx;
	IL_AddGroupObserver(mImageGroupContext, ImageGroupObserver, &mContext);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CBrowserContext::~CBrowserContext()
{
	// NOTE: IL_RemoveGroupObserver and DestroyImageContext are done on our behalf
	// in other places

	SetCompositor(NULL);

	// 97-06-06 mjc - remove the context from the global list in CNSContext::NoMoreUsers before calling mocha.
	//XP_RemoveContextFromList(&mContext);
	if (mContext.name != NULL)
		{
		XP_FREE(mContext.name);
		mContext.name = NULL;
		}

	// 98-05-27 pinkerton - call close callback
	if ( mCloseCallback )
		(*mCloseCallback)(mCloseCallbackArg);
}


//
// SetCloseCallback
//
// Set a callback (from the chrome structure) to be called when this window/context goes
// away.
//
void
CBrowserContext :: SetCloseCallback ( void (* close_callback)(void *close_arg), void* close_arg )
{
	mCloseCallback = close_callback;
	mCloseCallbackArg = close_arg;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::NoMoreUsers(void)
{
	mInNoMoreUsers = true;

		// this object is about to be destroyed...
	mIsRepaginationPending = false;	// ...so, clear requests for further work.

	CNSContext::NoMoreUsers();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SetCurrentView(CHTMLView* inView)
{
	if (inView != NULL)
		{
		}
	else
		{
		// Cleanup???
		}

// these are the old vars.  we want to make sure they are always null		
	mContext.fe.view = NULL;
	mContext.fe.newView = inView;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CBrowserContext* CBrowserContext::GetTopContext()
{
	CBrowserContext* theReturnContext = this;
	if (theReturnContext->mContext.grid_parent != NULL)
		{
		CBrowserContext* theTempContext = ExtractBrowserContext(theReturnContext->mContext.grid_parent);
		theReturnContext = theTempContext->GetTopContext();
		}
		
	return theReturnContext;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::HasColorSpace(void) const
{
	return (mContext.color_space != NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::HasGridParent(void) const
{
	return (mContext.grid_parent != NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::HasFullPagePlugin(void) const
{
	NPEmbeddedApp* theApp = mContext.pluginList;
	return ((theApp != NULL) && (theApp->next == NULL) && (theApp->pagePluginType == NP_FullPage));
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SetLoadImagesOverride(Boolean inOverride)
{
	mLoadImagesOverride = inOverride;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::IsLoadImagesOverride(void) const
{
	return mLoadImagesOverride;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SetDelayImages(Boolean inDelay)
{
	mDelayImages = inDelay;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::IsImageLoadingDelayed(void) const
{
	return mDelayImages;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean	CBrowserContext::IsRestrictedTarget(void) const
{
	return (mContext.restricted_target) ? true : false;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SetRestrictedTarget(Boolean inIsRestricted)
{
	mContext.restricted_target = inIsRestricted;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::IsRootDocInfoContext()
{
	// 1997-05-02 pkc -- I observe that the first context that gets called
	// for doc info window has a name of cDocInfoWindowContextName and has
	// NULL grid_parent && NULL grid_children fields.
	// Obviously, if this ever changes, this function needs to change.
	if (mContext.name != NULL)
		return (IsDocInfoWindow(mContext.name) && !HasGridParent()/* && !HasGridChildren()*/);
	else
		return false;
}

// 1997-05-16 pkc -- added this method to work around page source resize bug
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::IsViewSourceContext()
{
	if (mContext.name != NULL)
		return (IsViewSourceWindow(mContext.name));
	else
		return false;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::IsSpecialBrowserContext()
{
	if (mContext.name != NULL)
		return IsSpecialBrowserWindow(mContext.name);
	else
		return false;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::SupportsPageServices()
{
	return SHIST_CurrentHandlesPageServices(*this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- LAYERS / COMPOSITOR ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::HasCompositor(void) const
{
	return (mContext.compositor != NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CL_Compositor* CBrowserContext::GetCompositor(void) const
{
	return mContext.compositor;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SetCompositor(CSharableCompositor* inCompositor)
{
	mContext.compositor = inCompositor ? *inCompositor : (CL_Compositor*)nil;
	if (mCompositor)
		mCompositor->RemoveUser(this);
	mCompositor = inCompositor;
	if (mCompositor)
		mCompositor->AddUser(this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

PRBool CBrowserContext::HandleLayerEvent(	
	CL_Layer*				inLayer, 
	CL_Event*				inEvent)
{
	PRBool result = PR_FALSE;
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	if (theCurrentView != NULL)
		result = theCurrentView->HandleLayerEvent(inLayer, inEvent);
	return result;
}

PRBool CBrowserContext::HandleEmbedEvent(	
	LO_EmbedStruct*			inEmbed, 
	CL_Event*				inEvent)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return PR_FALSE;
	return theCurrentView->HandleEmbedEvent(inEmbed, inEvent);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- HISTORY ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::RememberHistoryPosition(
	Int32					inH,
	Int32					inV)
{
#ifdef LAYERS
	LO_Element* theNearestElement = LO_XYToNearestElement(&mContext, inH, inV, NULL);
#else
	LO_Element* theNearestElement = LO_XYToNearestElement(&mContext, inH, inV);
#endif 

	if (theNearestElement != NULL)
		SHIST_SetPositionOfCurrentDoc(&mContext.hist, theNearestElement->lo_any.ele_id);
}

History_entry* CBrowserContext::GetNextHistoryEntry(void)
{
	return SHIST_GetNext(&mContext);
}

History_entry* CBrowserContext::GetPreviousHistoryEntry(void)
{
	return SHIST_GetPrevious(&mContext);
}

Boolean CBrowserContext::CanGoForward(void)
{
	return SHIST_CanGoForward(&mContext);
}

Boolean CBrowserContext::CanGoBack(void)
{
	return SHIST_CanGoBack(&mContext);
}

Boolean CBrowserContext::HasGridChildren(void)
{
	return (mContext.grid_children != NULL);
}

Boolean CBrowserContext::IsGridChild(void)
{
	return (mContext.is_grid_cell);
}

Boolean CBrowserContext::IsGridCell()
{
	return (mContext.is_grid_cell);
}

Boolean CBrowserContext::GoForwardInGrid(void)
{
	return LO_ForwardInGrid(&mContext);
}

Boolean CBrowserContext::GoBackInGrid(void)
{
	return LO_BackInGrid(&mContext);
}

void CBrowserContext::GoForward(void)
{
	if (!HasGridChildren() || !GoForwardInGrid())
		LoadHistoryEntry(index_GoForward);
}

void CBrowserContext::GoBack(void)
{
	if (!HasGridChildren() || !GoBackInGrid())
		LoadHistoryEntry(index_GoBack);
}

void CBrowserContext::GoForwardOneHost()
{
	History*		history = &mContext.hist;
	History_entry*	currentEntry = GetCurrentHistoryEntry();
	History_entry*	entry = nil;
	
	if (currentEntry)
	{
		CAutoPtrXP<char> currentHost = NET_ParseURL(currentEntry->address, GET_HOST_PART);

		if (currentHost.get())
		{
			Int32 historyIndex = 0;
			Int32 currentEntryIndex = SHIST_GetIndex(history, currentEntry);
			Int32 numItemsInHistory = GetHistoryListCount();
			
			for (int i = currentEntryIndex + 1;  i <= numItemsInHistory && !historyIndex; i++)
			{
				entry = SHIST_GetEntry(history, i - 1);
				
				if (entry)
				{
					CAutoPtrXP<char> host = NET_ParseURL(entry->address, GET_HOST_PART);
					
					if (host.get())
					{
						if (strcasecomp(currentHost.get(), host.get()) != 0)
						{
							historyIndex = i;
						}
					}
				}
			}
			
			if (!historyIndex)
			{
				historyIndex = numItemsInHistory;
			}

			LoadHistoryEntry(historyIndex);
		}
	}
}

void CBrowserContext::GoBackOneHost()
{
	History*		history = &mContext.hist;
	History_entry*	currentEntry = GetCurrentHistoryEntry();
	History_entry*	entry = nil;
	
	if (currentEntry)
	{
		CAutoPtrXP<char> currentHost = NET_ParseURL(currentEntry->address, GET_HOST_PART);

		if (currentHost.get())
		{
			Int32 historyIndex = 0;
			Int32 currentEntryIndex = SHIST_GetIndex(history, currentEntry);
			
			for (int i = currentEntryIndex - 1;  i >= 1 && !historyIndex; i--)
			{
				entry = SHIST_GetEntry(history, i - 1);
				
				if (entry)
				{
					CAutoPtrXP<char> host = NET_ParseURL(entry->address, GET_HOST_PART);
					
					if (host.get())
					{
						if (strcasecomp(currentHost.get(), host.get()) != 0)
						{
							historyIndex = i;
						}
					}
				}
			}
			
			if (!historyIndex)
			{
				historyIndex = 1;
			}

			LoadHistoryEntry(historyIndex);
		}
	}
}

// inIndex is one-based
// special case:
// 		-1 is go back
//		0 is go forward
//		-2 is reload current
void CBrowserContext::LoadHistoryEntry(Int32 inIndex, Boolean inSuperReload)
{
	History_entry* entry = NULL;
	switch (inIndex)
	{
		case index_GoBack:
			entry = GetPreviousHistoryEntry();
			break;
		case index_GoForward:
			entry = GetNextHistoryEntry();
			break;
		case index_Reload:
			entry = GetCurrentHistoryEntry();
			break;
		default:
			entry = SHIST_GetObjectNum(&mContext.hist, inIndex);
	}
	if (entry)
	{
		URL_Struct* url = SHIST_CreateURLStructFromHistoryEntry(&mContext, entry); // SIGH!
		if ( url )
		{
			// 97-05-14 pkc -- only fiddle with url->force_reload if we're doing a reload
			if (inIndex == index_Reload)
			{
				if(inSuperReload)
					url->force_reload = NET_SUPER_RELOAD;
				else
					url->force_reload = NET_NORMAL_RELOAD;
			}
			SwitchLoadURL( url, FO_CACHE_AND_PRESENT );
		}
	}
}

void CBrowserContext::InitHistoryFromContext( CBrowserContext *parentContext)
{
	if ( parentContext )
	{
		SHIST_CopySession( &mContext, parentContext->operator MWContext*() );
		Int32 curIndex = parentContext->GetIndexOfCurrentHistoryEntry();
		SHIST_SetCurrent( &mContext.hist, curIndex );
	}
}

#pragma mark --- Image Observer ---

Boolean CBrowserContext::IsContextLoopingRecurse()
{
   int i = 1;
   MWContext *childContext;

   if (mImagesLooping)
	   return true;

   if (mMochaImagesLooping)
	   return true;

   while ((childContext = (MWContext *)XP_ListGetObjectNum(mContext.grid_children, i++)) != 0)
   {
	   if (ExtractBrowserContext(childContext)->IsContextLoopingRecurse())
		   return true;
   }
   
   return false;
}

			// Returns true if this context or its children have any looping images.
Boolean CBrowserContext::IsContextLooping()
{
	return (IsContextLoopingRecurse());
}


void
CBrowserContext::SetImagesLoading(Boolean inValue)
{
	mImagesLoading = inValue;
	LCommander::SetUpdateCommandStatus(true);
}

void
CBrowserContext::SetImagesLooping(Boolean inValue)
{
	mImagesLooping = inValue;
	LCommander::SetUpdateCommandStatus(true);
}

void
CBrowserContext::SetImagesDelayed(Boolean inValue)
{
	mImagesDelayed = inValue;
	LCommander::SetUpdateCommandStatus(true);
}

void
CBrowserContext::SetMochaImagesLoading(Boolean inValue)
{
	mMochaImagesLoading = inValue;
	LCommander::SetUpdateCommandStatus(true);
}

void
CBrowserContext::SetMochaImagesLooping(Boolean inValue)
{
	mMochaImagesLooping = inValue;
	LCommander::SetUpdateCommandStatus(true);
}

void
CBrowserContext::SetMochaImagesDelayed(Boolean inValue)
{
	mMochaImagesDelayed = inValue;
	LCommander::SetUpdateCommandStatus(true);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- URL MANIPULATION ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// Moved to CNSContext


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- REPAGINATION ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::Repaginate(NET_ReloadMethod repage)
{
	History_entry* theCurrentEntry = SHIST_GetCurrent(&mContext.hist);
	if ((theCurrentEntry == NULL) || (theCurrentEntry->is_binary))
		{
		BroadcastMessage(msg_NSCPEmptyRepagination);
		return;
		}
		
	if (XP_IsContextBusy(&mContext))
		{
		mIsRepaginationPending = true;
		return;
		}

	BroadcastMessage(msg_NSCPAboutToRepaginate);

	// it would be bad to reload the URL for the editor (file may not be saved!)
	if (EDT_IS_EDITOR((&mContext)))
	{
		// Editor can relayout page without having to do NET_GetURL
		BroadcastMessage(msg_NSCPEditorRepaginate);
		return;
	}
	
	URL_Struct* theURL = SHIST_CreateWysiwygURLStruct(&mContext, theCurrentEntry);	
	if (theURL == NULL)
		return;
	
	theURL->force_reload = repage;
	
	// don't reload any images! Only from cache!
	mIsRepaginationPending = false;
	mIsRepaginating = true;

	NPL_SamePage(&mContext);
	SwitchLoadURL(theURL, FO_CACHE_AND_PRESENT);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean	CBrowserContext::IsRepaginating(void) const
{
	return mIsRepaginating;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserContext::IsRepagintaitonPending(void) const
{
	return mIsRepaginationPending;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- FRAME MANAGEMENT ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

MWContext* CBrowserContext::CreateGridContext(
	void* 					inHistList,
	void* 					inHistEntry,
	Int32					inX,
	Int32 					inY,
	Int32					inWidth,
	Int32					inHeight,
	char* 					inURLString,
	char* 					inWindowTarget,
	Int8					inScrollMode,
	NET_ReloadMethod 		inForceReload,
	Bool 					inNoEdge)
{
	Assert_(mContext.type != MWContextText);

	CBrowserContext* theNewContext = NULL;

	try
		{
		theNewContext = new CBrowserContext(*this);
		StSharer theShareLock(theNewContext);
		
		theNewContext->mContext.is_grid_cell = true;
// 	theNewContext->fe.realContext = owningContext;
		theNewContext->mContext.grid_parent = *this;

		CHTMLView* theCurrentView = ExtractHyperView(*this);
		Assert_(theCurrentView != NULL);
		ThrowIfNULL_(theCurrentView);
		theCurrentView->CreateGridView(theNewContext, inX, inY, inWidth, inHeight, inScrollMode, inNoEdge);
		
		// ¥ create the new context for our view
//		vcontext = newContext = context->CreateGridContext();
//		newContext->fe.view = newView;
//		newContext->grid_parent = fHContext;

		if (inWindowTarget != NULL)
			theNewContext->SetDescriptor(inWindowTarget);

		XP_AddContextToList(*theNewContext);
				
		// ¥ grid history magic
		XP_List* theHistList = (XP_List*)inHistList;
		History_entry* theHist = (History_entry*)inHistEntry;
		if (inHistList != NULL)
			{
			if (theNewContext->mContext.hist.list_ptr != NULL)
				XP_FREE(theNewContext->mContext.hist.list_ptr);
			theNewContext->mContext.hist.list_ptr = theHistList;
			}
		else if (theHist != NULL)
			SHIST_AddDocument(*theNewContext, theHist);
		if (theHist == NULL)
			{
			URL_Struct* theURL = NET_CreateURLStruct(inURLString, NET_DONT_RELOAD);
			theURL->force_reload = inForceReload;

			History_entry* theNewHist = SHIST_GetCurrent( &mContext.hist );
			if ((theNewHist != NULL) && (theNewHist->referer != NULL))
				theURL->referer = XP_STRDUP(theNewHist->referer);
			
			theNewContext->ImmediateLoadURL(theURL, FO_CACHE_AND_PRESENT);
			}
		else
			{
			URL_Struct* theURL = SHIST_CreateURLStructFromHistoryEntry(*theNewContext, theHist);
			if (theURL)
				theNewContext->ImmediateLoadURL(theURL, FO_CACHE_AND_PRESENT);
			}
		
		theNewContext->AddUser(this);
		BroadcastMessage(msg_NSCGridContextCreated, theNewContext);
		}
	catch (...)
		{
		
		return NULL;
		}

	return *theNewContext;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void* CBrowserContext::DisposeGridContext(
	XP_Bool					inSaveHistory)
{
	// Fix bug #76484 -- remove multiple calls to LO_DiscardDocument. It will
	// get called in CNSContext::NoMoreUsers()
	// LO_DiscardDocument(&mContext);

	if (!inSaveHistory)
		{
		// We will call this again in ~CNSContext, but it looks safe to call
		// again because the first call will set mContext.hist.list_ptr to zero.
		// We need to call this here because we might try to destroy form
		// elements in the history, which are destroyed by a callback through
		// the associated CHTMLView; but the broadcast below will destroy the
		// CHTMLView.
		SHIST_EndSession(&mContext);
		}
	
	XP_List* theHistoryList = NULL;
	if (inSaveHistory)
		{
		theHistoryList = mContext.hist.list_ptr;
		mContext.hist.list_ptr = NULL;
		}

	Assert_(mContext.grid_parent != NULL);
	CBrowserContext* theParentContext = ExtractBrowserContext(mContext.grid_parent);
	Assert_(theParentContext != NULL);
	
	theParentContext->DisposeGridChild(this);
	
	// Moved broadcast past DisposeGridChild() so that the view has the last shared reference
	// on the context. Thus, the view is still valid when the context is actually disposed,
	// and callback such as HideJavaAppElement will still work.
	// Now that this broadcast happens after the call to DisposeGridChild(), a more
	// appropriate message would be msg_NSCGridContextPostDispose.
	
	// First we broadcast to an client views that this grid context is about to die
	// clients should clean up and remove their shared references to the context
	// upon receiving this message.
	BroadcastMessage(msg_NSCGridContextPreDispose, &inSaveHistory);

	return theHistoryList;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisposeGridChild(
	CBrowserContext*				inChildContext)
{
	Assert_(inChildContext != this);
	Assert_(inChildContext != NULL);

	// 97.12.19 pchen. Fix AT&T bug #97213. If we're removing all our
	// grid children, reset scroll mode of our corresponding CHTMLView.
	if (CountGridChildren() == 1)
	{
		// removing grid children, reset scroll mode on CHTMLView
		CHTMLView* theCurrentView = ExtractHyperView(*this);
		if (theCurrentView != NULL)
		{
			theCurrentView->ResetToDefaultScrollMode();
		}
	}

	inChildContext->RemoveUser(this);
	BroadcastMessage(msg_NSCGridContextDisposed);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::ReloadGridFromHistory(
	void* 					inHistEntry,
	NET_ReloadMethod 		inReload)
{
	History_entry*	theHist = (History_entry*)inHistEntry;
	if (theHist == NULL)
		return;
			
	URL_Struct*	theURL = SHIST_CreateURLStructFromHistoryEntry(*this, theHist);
	theURL->force_reload = inReload;
	
	SwitchLoadURL(theURL, FO_CACHE_AND_PRESENT);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::RestructureGridContext(
	Int32					inX,
	Int32 					inY,
	Int32					inWidth,
	Int32					inHeight)
{
	Assert_(mContext.type != MWContextText);
	
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->RestructureGridView(inX, inY, inWidth, inHeight);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::GetFullGridSize(
	Int32&					outWidth,
	Int32&					outHeight)
{
	outWidth = outHeight = 80;
	if (mContext.type != MWContextText)
		{
		CHTMLView* theCurrentView = ExtractHyperView(*this);
		Assert_(theCurrentView != NULL);
		if (theCurrentView)
			theCurrentView->GetFullGridSize(outWidth, outHeight);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int32 CBrowserContext::CountGridChildren(void) const
{
	Int32 theChildCount = 0;
	if (mContext.grid_children != NULL)
		theChildCount = XP_ListCount(mContext.grid_children);
		
	return theChildCount;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- CALLBACK IMPLEMENTATION ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::AllConnectionsComplete(void)
{
	mIsRepaginating = false;
	
	CNSContext::AllConnectionsComplete();
	
	if (CWebFontReference::NeedToReload(*this))
	{
		LO_InvalidateFontData(*this);
		this->Repaginate();
	}
		
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::LayoutNewDocument(
	URL_Struct*			inURL,
	Int32*				inWidth,
	Int32*				inHeight,
	Int32*				inMarginWidth,
	Int32*				inMarginHeight)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	// Check to see if theCurrentView is NULL because it's possible
	// for XP code to try to lay out after we've destroyed window
	if (theCurrentView != NULL)
	{
		theCurrentView->LayoutNewDocument(inURL, inWidth, inHeight, inMarginWidth, inMarginHeight);
		
		CStr31 theTitle = CFileMgr::FileNameFromURL(inURL->address);
		History_entry* theNewEntry = SHIST_CreateHistoryEntry(inURL, theTitle);
		SHIST_AddDocument(*this, theNewEntry);
		
		BroadcastMessage(msg_NSCLayoutNewDocument, inURL);
		
		// setup security status
		if (!IsGridChild())
		{
			int status = XP_GetSecurityStatus(*this);
			BroadcastMessage(msg_SecurityState, (void*)status);
		}
	}
	// 97-06-04 pkc -- clear default status string in context
	ClearDefaultStatus();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
	
void CBrowserContext::SetDocTitle(
	char* 					inTitle)
{
	// 97-05-20 pkc -- don't broadcast if we're in NoMoreUsers() call chain
	// only broadcast doc title changed if we're top-level context
	if (!mInNoMoreUsers && !IsGridChild())
	{
        if(inTitle != NULL)      
        {
			unsigned char *converted =
					INTL_ConvertLineWithoutAutoDetect(
							INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(&mContext)),
							ScriptToEncoding(::FontToScript(applFont)),
							(unsigned char *) inTitle,
							(uint32) XP_STRLEN(inTitle));
			if (converted && ((char *) converted != inTitle))
			{
				inTitle = (char *) converted;
			} 

			StrAllocCopy(mContext.title, inTitle);
			SHIST_SetTitleOfCurrentDoc(&mContext);
			BroadcastMessage(msg_NSCDocTitleChanged, inTitle);
			if (converted)
				XP_FREE(converted);
		}
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::FinishedLayout(void)
{
	FM_SetFlushable(&mContext, true);

	if (!mInNoMoreUsers)
		BroadcastMessage(msg_NSCFinishedLayout);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int CBrowserContext::GetTextInfo(
	LO_TextStruct*			inText,
	LO_TextInfo*			inTextInfo)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return 1;
	return theCurrentView->GetTextInfo(inText, inTextInfo);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int CBrowserContext::MeasureText(
	LO_TextStruct*			inText,
	short*					outCharLocs)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return 0;
	return theCurrentView->MeasureText(inText, outCharLocs);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::GetEmbedSize(
	LO_EmbedStruct*			inEmbedStruct,
	NET_ReloadMethod		inReloadMethod)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->GetEmbedSize(inEmbedStruct, inReloadMethod);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::GetJavaAppSize(
	LO_JavaAppStruct*		inJavaAppStruct,
	NET_ReloadMethod		inReloadMethod)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->GetJavaAppSize(inJavaAppStruct, inReloadMethod);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::GetFormElementInfo(
	LO_FormElementStruct* 	inElement)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->GetFormElementInfo(inElement);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::GetFormElementValue(
	LO_FormElementStruct* 	inElement,
	XP_Bool 				inHide)
{
	/*
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	theCurrentView->GetFormElementValue(inElement, inHide);
	*/
	// Check that the view is not deleted, because the form elements
	// try to access panes within that view.
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	if (theCurrentView)
		UFormElementFactory::GetFormElementValue(inElement, inHide);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::ResetFormElement(
	LO_FormElementStruct* 	inElement)
{
	/*
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	theCurrentView->ResetFormElement(inElement);
	*/
	UFormElementFactory::ResetFormElement(inElement, true, true);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SetFormElementToggle(
	LO_FormElementStruct* 	inElement,
	XP_Bool 				inToggle)
{
	/*
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	theCurrentView->SetFormElementToggle(inElement, inToggle);
	*/
	UFormElementFactory::SetFormElementToggle(inElement, inToggle);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::FreeEmbedElement(
	LO_EmbedStruct*			inEmbedStruct)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->FreeEmbedElement(inEmbedStruct);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::CreateEmbedWindow(
	NPEmbeddedApp*		inEmbeddedApp)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->CreateEmbedWindow(inEmbeddedApp);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SaveEmbedWindow(
	NPEmbeddedApp*		inEmbeddedApp)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->SaveEmbedWindow(inEmbeddedApp);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::RestoreEmbedWindow(
	NPEmbeddedApp*		inEmbeddedApp)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->RestoreEmbedWindow(inEmbeddedApp);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DestroyEmbedWindow(
	NPEmbeddedApp*		inEmbeddedApp)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->DestroyEmbedWindow(inEmbeddedApp);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::FreeJavaAppElement(
	LJAppletData*			inAppletData)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->FreeJavaAppElement(inAppletData);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::HideJavaAppElement(
	LJAppletData*				inAppletData)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->HideJavaAppElement(inAppletData);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::FreeEdgeElement(
	LO_EdgeStruct*			inEdgeStruct)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->FreeEdgeElement(inEdgeStruct);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::FormTextIsSubmit(
	LO_FormElementStruct* 	inElement)
{
	/*
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	theCurrentView->FormTextIsSubmit(inElement);
	*/
	UFormElementFactory::FormTextIsSubmit(inElement);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplaySubtext(
	int 					inLocation,
	LO_TextStruct*			inText,
	Int32 					inStartPos,
	Int32					inEndPos,
	XP_Bool 				inNeedBG)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplaySubtext(inLocation, inText, inStartPos, inEndPos, inNeedBG);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayText(
	int 					inLocation,
	LO_TextStruct*			inText,
	XP_Bool 				inNeedBG)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayText(inLocation, inText, inNeedBG);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayEmbed(
	int 					inLocation,
	LO_EmbedStruct*			inEmbedStruct)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayEmbed(inLocation, inEmbedStruct);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayJavaApp(
	int 					inLocation,
	LO_JavaAppStruct*		inJavaAppStruct)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayJavaApp(inLocation, inJavaAppStruct);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayEdge(
	int 					inLocation,
	LO_EdgeStruct*			inEdgeStruct)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayEdge(inLocation, inEdgeStruct);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayTable(
	int 					inLocation,
	LO_TableStruct*			inTableStruct)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayTable(inLocation, inTableStruct);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayCell(
	int 					inLocation,
	LO_CellStruct*			inCellStruct)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayCell(inLocation, inCellStruct);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::InvalidateEntireTableOrCell(
	LO_Element*			inElement)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->InvalidateEntireTableOrCell(inElement);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayAddRowOrColBorder(
	XP_Rect*				inRect,
	XP_Bool					inDoErase)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayAddRowOrColBorder(inRect, inDoErase);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplaySubDoc(
	int 					/*inLocation*/,
	LO_SubDocStruct*		/*inSubdocStruct*/)
{
	// Empty NS_xxx implementation
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayLineFeed(
	int 					inLocation,
	LO_LinefeedStruct*		inLinefeedStruct,
	XP_Bool 				inNeedBG)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayLineFeed(inLocation, inLinefeedStruct, inNeedBG);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayHR(
	int 					inLocation,
	LO_HorizRuleStruct*		inRuleStruct)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayHR(inLocation, inRuleStruct);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayBullet(
	int 					inLocation,
	LO_BullettStruct*		inBullettStruct)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayBullet(inLocation, inBullettStruct);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayFormElement(
	int 					inLocation,
	LO_FormElementStruct* 	inFormElement)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayFormElement(inLocation, inFormElement);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayBorder(
	int 					inLocation,
	int						inX,
	int						inY,
	int						inWidth,
	int						inHeight,
	int						inBW,
	LO_Color*	 			inColor,
	LO_LineStyle			inStyle)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayBorder(inLocation, inX, inY, inWidth, inHeight, inBW, inColor, inStyle);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	UpdateEnableStates--Editor can change enable/disable state of buttons
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::UpdateEnableStates()
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->UpdateEnableStates();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DisplayFeedback--Editor shows selection
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DisplayFeedback(
	int 					inLocation,
	LO_Element_struct		*inElement)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->DisplayFeedback(inLocation, inElement);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::ClearView(
	int 					inWhich)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->ClearView(inWhich);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SetDocDimension(
	int 					inLocation,
	Int32 					inWidth,
	Int32 					inLength)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	// Check to see if theCurrentView is NULL because it's possible
	// for XP code to try to lay out after we've destroyed window
	if (theCurrentView != NULL)
		theCurrentView->SetDocDimension(inLocation, inWidth, inLength);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SetDocPosition(
	int 					inLocation,
	Int32 					inX,
	Int32 					inY)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	// Check to see if theCurrentView is NULL because it's possible
	// for XP code to try to lay out after we've destroyed window
	if (theCurrentView != NULL)
		theCurrentView->SetDocPosition(inLocation, inX, inY);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::GetDocPosition(
	int 					inLocation,
	Int32*					outX,
	Int32*					outY)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->GetDocPosition(inLocation, outX, outY);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SetBackgroundColor(
	Uint8 					inRed,
	Uint8					inGreen,
	Uint8 					inBlue)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	theCurrentView->SetBackgroundColor(inRed, inGreen, inBlue);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::EraseBackground(
	int						inLocation,
	Int32					inX,
	Int32					inY,
	Uint32					inWidth,
	Uint32					inHeight,
	LO_Color*				inColor)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->EraseBackground(inLocation, inX, inY, inWidth, inHeight, inColor);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::SetDrawable(
	CL_Drawable*			inDrawable)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	CDrawable *theDrawable;
	
	Assert_(theCurrentView != NULL);
	if (theCurrentView == NULL)
		return;
	if (inDrawable)
		theDrawable = (CDrawable *) CL_GetDrawableClientData(inDrawable);
	else
		theDrawable = NULL;
	theCurrentView->SetCurrentDrawable(theDrawable);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::GetTextFrame(
	LO_TextStruct*			inTextStruct,
	Int32					inStartPos,
	Int32					inEndPos,
	XP_Rect*				outFrame)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->GetTextFrame(inTextStruct, inStartPos, inEndPos, outFrame);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::GetDefaultBackgroundColor(
	LO_Color* outColor) const
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->GetDefaultBackgroundColor(outColor);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::DrawJavaApp(
	int 					inLocation,
	LO_JavaAppStruct*		inJavaAppStruct)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->DrawJavaApp(inLocation, inJavaAppStruct);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::HandleClippingView(
	struct LJAppletData 	*appletD, 
	int 					x, 
	int 					y, 
	int 					width, 
	int 					height)
{
	CHTMLView* theCurrentView = ExtractHyperView(*this);
	Assert_(theCurrentView != NULL);
	if (theCurrentView != NULL)
		theCurrentView->HandleClippingView(appletD, x, y, width, height);
}

CSharableCompositor::~CSharableCompositor()
{
	if (mCompositor)
		CL_DestroyCompositor(mCompositor);
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::ConstructJSDialogTitle(LStr255& outTitle)
{
	if (mContext.url)
	{
		AppendJSStringToHost(outTitle, mContext.url);
	}
	else
	{
		History_entry* hist = GetCurrentHistoryEntry();
		if (hist)
		{
			AppendJSStringToHost(outTitle, hist->address);
		}
		else
		{
			AppendJSStringToHost(outTitle, NULL);
		}
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserContext::Alert(
	const char*				inAlertText)
{
	CStr255 	pmessage( inAlertText );
	StripDoubleCRs( pmessage );
	pmessage=NET_UnEscape(pmessage);
	if (mContext.bJavaScriptCalling)
	{
		// this is a JavaScript alert, make title string for alert
		LStr255 titleStr;
		ConstructJSDialogTitle(titleStr);
		UStdDialogs::Alert(pmessage, eAlertTypeCaution, NULL, NULL, &titleStr);
	}
	else
	{
		UStdDialogs::Alert(pmessage, eAlertTypeCaution);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

XP_Bool	CBrowserContext::Confirm(
	const char* 			inMessage)
{
	XP_Bool result;
	CStr255 mesg(inMessage);
	ConvertCRtoLF(mesg);
	StripDoubleCRs(mesg);
	mesg = NET_UnEscape(mesg);
	if (mContext.bJavaScriptCalling)
	{
		// this is a JavaScript confirm, make title string for confirm
		LStr255 titleStr;
		ConstructJSDialogTitle(titleStr);
		result = UStdDialogs::AskOkCancel(mesg, NULL, NULL, &titleStr);
	}
	else
	{
		result = UStdDialogs::AskOkCancel(mesg);
	}
	return result;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char* CBrowserContext::Prompt(
	const char* 			inMessage,
	const char*				inDefaultText)
{
	char* result = NULL;
	CStr255 mesg(inMessage), ioString(inDefaultText);
	mesg = NET_UnEscape(mesg);
	Boolean prompt = false;

	if (mContext.bJavaScriptCalling)
	{
		// this is a JavaScript prompt, make title string for prompt
		LStr255 titleStr;
		ConstructJSDialogTitle(titleStr);
		CStr255 title(titleStr);
		prompt = UStdDialogs::AskStandardTextPrompt(title, mesg, ioString);
	}
	else
	{
		prompt = UStdDialogs::AskStandardTextPrompt("", mesg, ioString);
	}
	
	if (prompt)
	{
		if (ioString.Length() > 0)
		{
			result = (char*)XP_ALLOC(ioString.Length() + 1);
			ThrowIfNULL_(result);
			::BlockMoveData(&ioString[1], result, ioString.Length());
			result[ioString.Length()] = '\0';
		}
	}

	return result;	
}
