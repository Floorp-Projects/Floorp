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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CHTMLView.cp
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "CHTMLView.h"


#include "CStLayerOriginSetter.h"

#include <algorithm>

#include "CHTMLClickrecord.h"
#include "CBrowserContext.h"
#include "CBrowserWindow.h"
#include "CURLDispatcher.h"
#include "UFormElementFactory.h"
#include "RandomFrontEndCrap.h"
#include "CDrawable.h"
#include "CMochaHacks.h" // this includes libevent.h
#include "CHyperScroller.h"
#include "UGraphicGizmos.h"
#include "CContextMenuAttachment.h"
#include "CSharedPatternWorld.h"
#include "CApplicationEventAttachment.h"
#include "CViewUtils.h"
#include "CTargetFramer.h"
#include "uapp.h"

#include "libimg.h"
//#include "allxpstr.h" THE DATA DEFINITION MOVED TO StringLibPPC
#include "xlate.h"
#include "xp_rect.h"
#include "xp_core.h"

#include "layers.h"
#include "nppriv.h"

// FIX ME!!! this is only for SCROLLER_ID  remove in favor of xxx::class_ID
#include "resgui.h" 
#include "macgui.h"
#include "mimages.h"
#include "mplugin.h"

#if defined (JAVA)
#include "mjava.h"
#endif /* defined (JAVA) */

#include "mforms.h"
#include "mkutils.h"

#include "libi18n.h"

#include "prefapi.h"
#include "secnav.h"

extern "C" {
#include "typedefs_md.h"

#if defined (JAVA)
#include "native.h"
#include "sun_awt_macos_MComponentPeer.h"
#include "java_awt_Component.h"
#include "interpreter.h"
#include "exceptions.h"
#include "prlog.h"
#endif /* defined (JAVA) */

#undef __cplusplus
#define IMPLEMENT_netscape_applet_MozillaAppletContext
#include "n_applet_MozillaAppletContext.h"
// #include "mdmacmem.h"
#include "java.h"
#include "jri.h"
#include "lj.h"
}

#if defined (JAVA)
#include "MToolkit.h"
#include "MFramePeer.h"
#endif /* defined (JAVA) */

#include "mprint.h"					// 97/01/24 jrm
#include "ufilemgr.h"				// 96-12-16 deeje
#include "uerrmgr.h"
#include "CBookmarksAttachment.h"
#include "Drag.h"
#include "macutil.h"
#include "UStdDialogs.h"
#include "findw.h"
#include "CPaneEnabler.h"
#include "msgcom.h"	// for MSG_MailDocument

#include <UDrawingState.h>
#include <UMemoryMgr.h>
#include <UCursor.h>
#include <Sound.h>

const Int16 FocusBox_Size		=	3;

const Int16 TableBorder_TintLevel	=	20000;

const ResIDT cPaulsMiscStrListID = 16010;
const Int16 cSendPageIndex = 4;
const Int16 cSendFrameIndex = 5;

	// Explicit Template instantiation for StValueChanger for LO_Element*

template <class T>
StValueChanger<T>::StValueChanger(
	T	&ioVariable,
	T	inNewValue)
		: mVariable(ioVariable),
		  mOrigValue(ioVariable)
{
	ioVariable = inNewValue;
}


template <class T>
StValueChanger<T>::~StValueChanger()
{
	mVariable = mOrigValue;
}

template class StValueChanger<LO_Element*>;

// Utility function for table borders
Uint16 AddWithoutOverflow(Uint16 base, Uint16 addition);
Uint16 SubWithoutUnderflow(Uint16 base, Uint16 difference);
void ComputeBevelColor(Boolean inRaised, RGBColor inBaseColor, RGBColor& outBevelColor);

static void MochaFocusCallback(MWContext * pContext, LO_Element * lo_element, int32 lType, void * whatever, ETEventStatus status);

Boolean HasEventHandler(MWContext *context, uint32 events);

/* returns true if any of the events specified has a handler in the context and its parents. */
Boolean HasEventHandler(MWContext *context, uint32 events)
{
	if (context == NULL) return false;
    if (context->event_bit & events)
		return true;
    if (context->grid_parent)
		return HasEventHandler(context->grid_parent, events);
    return false;
}

void SafeSetCursor(ResIDT	inCursorID)
{
	CursPtr		cursorPtr;
	CursHandle	cursorHandle;

	cursorHandle = ::GetCursor(inCursorID);
	if (cursorHandle != NULL) {
		cursorPtr = *cursorHandle;
	}
	else {
		cursorPtr = &UQDGlobals::GetQDGlobals()->arrow;
	}
	
	::SetCursor(cursorPtr);
}

UInt32 CHTMLView::sLastFormKeyPressDispatchTime = 0; // to prevent infinite event dispatching loop
Boolean CHTMLView::sCachedAlwaysLoadImages = false;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
		
CHTMLView::CHTMLView(LStream* inStream)
	:	LView(inStream)
	,	LDragAndDrop(GetMacPort(), this)
	,	mContext(nil)
	,	mSuperHTMLView(nil)
	,	mCompositor(nil)
	, mPendingDocDimension_IsValid(false)
	,	mCurrentClickRecord(nil)
	,	mDragElement(nil)
	,	mPatternWorld(nil)
	,	mNeedToRepaginate(false)
	,	mDontAddGridEdgeToList(false)
	,	mLoadingURL(false)
	,	mStopEnablerHackExecuted(false)
	,	mInFocusCallAlready(false)
{

	// FIX ME: Use C++.  I.e., use mem-initializers where possible (as above)
	mNoBorder = false;
	mCachedPort = NULL;
	mDefaultScrollMode = LO_SCROLL_AUTO;
	mScrollMode = mDefaultScrollMode;
	mHasGridCells = false;
	mShowFocus = false;
	mElemBaseModel = NULL;
	mBackgroundImage = NULL;
	mEraseBackground = TRUE;
	memset(&mBackgroundColor, 0xFF, sizeof(mBackgroundColor)); // white

	mCompositor = NULL;
	mLayerClip = NULL;
	::SetEmptyRgn(mSaveLayerClip);

	mSendDataUPP = NewDragSendDataProc(LDropArea::HandleDragSendData);
	Assert_(mSendDataUPP != NULL);
	
// FIX ME!!! the scroller dependency needs to be removed
	mScroller = NULL;
	
	mTimerURLString = NULL;

	mOffscreenDrawable = NULL;
	mOnscreenDrawable = NULL;
	mCurrentDrawable = NULL;
	mWaitMouseUp = false;
	
	mLayerOrigin.h = mLayerOrigin.v = 0;

	RegisterCallBackCalls();
	
	// Sets sCachedAlwaysLoadImages for the first time
	PrefInvalidateCachedPreference("general.always_load_images", (void *)this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CHTMLView::~CHTMLView()
{
	if (!GetSuperHTMLView())
	{
		UnregisterCallBackCalls();
	}
	if (mPatternWorld)
		mPatternWorld->RemoveUser(this);
	
	if (mSendDataUPP != NULL)
		DisposeRoutineDescriptor(mSendDataUPP);

	if ( mOnscreenDrawable != NULL )
		mOnscreenDrawable->SetParent ( NULL );

	SetContext(NULL);	
	//CL_DestroyCompositor(mCompositor);
	// brade:  check for NULL in case compositor wasn't created yet
	if ( mCompositor )
		mCompositor->RemoveUser(this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::FinishCreateSelf(void)
{
	LView::FinishCreateSelf();

	mCachedPort = GetMacPort();
	Assert_(mCachedPort != NULL);
	
	// ¥¥¥ Hack, how else do we do this though?
	// Main view or the frames

// FIX ME!!! the scroller dependency need to be removed
//	if ((mSuperView->GetPaneID() == SCROLLER_ID ) || (mSuperView->GetPaneID() == 1005))
//		mScroller = (CHyperScroller*)mSuperView;
	mScroller = dynamic_cast<CHyperScroller*>(mSuperView); // - jrm

	SDimension16 theFrameSize;
	GetFrameSize(theFrameSize);
		
	/* create our onscreen drawable that will route drawable method calls from the compositor */
	/* to this view */
	mOnscreenDrawable = new CRouterDrawable();
	if ( mOnscreenDrawable != NULL )
		{
		// send all onscreen drawable method calls this way
		mOnscreenDrawable->SetParent ( this );

		CL_Drawable * theDrawable = CL_NewDrawable(theFrameSize.width, theFrameSize.height, CL_WINDOW,
			 &mfe_drawable_vtable, (void *) mOnscreenDrawable);
		 
		if (theDrawable != NULL)
			{
			CL_Drawable* theOffscreen = NULL;
			
			// Create our offscreen drawable. If it fails for whatever reason, we don't really care
			// as we'll just always be onscreen
			mOffscreenDrawable = COffscreenDrawable::AllocateOffscreen();
			if ( mOffscreenDrawable != NULL )
				{
				/* have the offscreen defer all layers calls to us */
				mOffscreenDrawable->SetParent ( this );
				
				theOffscreen = CL_NewDrawable(theFrameSize.width, theFrameSize.height, CL_BACKING_STORE,
					&mfe_drawable_vtable, (void *) mOffscreenDrawable);
				}
		
			// now create our compositor
			CL_Compositor* c = CL_NewCompositor(theDrawable, theOffscreen, 0, 0, theFrameSize.width, theFrameSize.height, 10);
			mCompositor = new CSharableCompositor(c);
			mCompositor->AddUser(this); // shared by context and view.
			}
		}

	// set up background color and pattern
	// This code is here so that we display default background when
	// user has blank home page. - no, that's not the only reason.  PLEASE
	// TRY TO REMEMBER THAT THIS CLASS IS USED FOR MAIL MESSAGES AND COMPOSER VIEWS!
	// ¥ install the user's default solid background color & pattern
	InstallBackgroundColor();
	// now use installed background color to set window background color
	SetWindowBackgroundColor();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::SetContext(
	CBrowserContext*		inNewContext)
{
	// jrm - I added these bookend calls to disable the compositor - 97/02/15.
	//			callbacks from timer were crashing!
	if (!inNewContext && mCompositor)
		CL_SetCompositorEnabled(*mCompositor, PR_FALSE);
	if (mContext != NULL)
		{
		// mContext->SetCompositor(NULL);
				// ^^^^^^^^Do NOT do this.  If you do, layout will not be able
				// to clean up properly.  This is very bad.
				// The compositor needs to stick around till the context is deleted.
				// So the context shares the CSharableCompositor
		mContext->RemoveUser(this);
		// set pointer to this view in MWContext to NULL
		mContext->ClearMWContextViewPtr();
		}
		
	mContext = inNewContext;
		
	if (mContext != NULL)
		{
		// Set up the character set stuff
		SetFontInfo();

		mContext->SetCurrentView(this);
		mContext->SetCompositor(mCompositor); // context will share it.
		mContext->AddListener(this);
		mContext->AddUser(this);
		}
	if (inNewContext && mCompositor)
		CL_SetCompositorEnabled(*mCompositor, PR_TRUE);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- ACCESSORS ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CHTMLView::IsBorderless(void) const
{
	return (mNoBorder && !IsRootHTMLView());
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CHTMLView::IsRootHTMLView(void) const
{
	return (mSuperHTMLView == NULL);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CHTMLView::IsFocusedFrame(void) const
{
	return (mShowFocus && !mHasGridCells && IsOnDuty());
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CHTMLView* CHTMLView::GetSuperHTMLView(void)
{
	return mSuperHTMLView;
}

// ---------------------------------------------------------------------------
//		¥ BeTarget
// ---------------------------------------------------------------------------
/* since CHTMLView is an LTabGroup, its default behaviour would be to cycle
   among its subpanes until it found the next one that wanted to be the target.
   There can be situations, however, where none of our subpanes want to be
   the target; a silly impasse.  Our BeTarget then mimics the behaviour of
   LTabGroup::BeTarget, but is prepared to accept the focus itself if necessary.
     An added wrinkle comes from the inability of certain of our subpanes
   (CPluginView) to correctly preflight whether they will be able to accept
   the target (in ProcessCommand).  When this happens, CPluginView tells its
   supercommander (us) to be the target and a stack explosion results.
   Thus the member variable, mTargeting, which detects recursive calls*/

/* Later note: BeTarget was gutted so that CHTMLView _could_ take the focus.
   This solves problems that happen with simultaneous text selection in the
   CHTMLView body and in a form edit field.  If you wish to reinstate BeTarget(),
   you must declare a private class boolean called mTargeting, and initialize
   it to false in the constructor.
*/

void
CHTMLView::BeTarget() {

/*
	// punt if one of our [bleep]ing subcommanders is trying to put the onus back on us
	if (mTargeting)
		return;
	StValueChanger<Boolean>	targetMode (mTargeting, true);

	LCommander		*onDutySub = GetOnDutySub();
	Int32			pos,		// index of the subcommander currently handed the torch
					subCount = mSubCommanders.GetCount();

	pos = subCount;
	if (onDutySub)
		pos = mSubCommanders.FetchIndexOf (&onDutySub);

	// find the next willing subcommander in rotation (a la LTabGroup::BeTarget)
	if (pos > 0) {
		Int32		startingPos = pos;
		LCommander	*newTarget;

		do {
			// increment and wrap
			if (++pos > subCount)
				pos = 1;
			mSubCommanders.FetchItemAt (pos, &newTarget);

			if (newTarget->ProcessCommand(msg_TabSelect)) {
				if (newTarget->IsTarget())
					break;
			}
		} while (pos != startingPos);
	}
*/

}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	Shows the pane by scrolling to it
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::ShowView( LPane& pane )
{
	if (mScroller && mScroller->HasVerticalScrollbar())

// ¥¥¥ FIX ME. We really need to know which way do we want to scroll,
// and then constrain it based on whether we have scrollbars
	{
		SPoint32 topLeft, botRight;
		SDimension16 size;
		pane.GetFrameLocation(topLeft);	// This is in window coordinates
		pane.GetFrameSize(size);
		topLeft.v -= mImageLocation.v;	// Adjust for our view's image position
		topLeft.h -= mImageLocation.h;
		botRight.h = topLeft.h + size.width;
		botRight.v = topLeft.v + size.height;
		RevealInImage(this, topLeft, botRight);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::SetRepaginate(Boolean inSetting)
{
	mNeedToRepaginate = inSetting;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CHTMLView::GetRepaginate()
{
	return mNeedToRepaginate;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::SetSuperHTMLView(CHTMLView *inView)
{
	// If we have a SuperHTMLView then we don't want to be called back.
	if (inView && !mSuperHTMLView)
	{
		UnregisterCallBackCalls();
	}
	else if (!inView && mSuperHTMLView)
	{
		RegisterCallBackCalls();
	}
	mSuperHTMLView = inView;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::SetFormElemBaseModel(LModelObject* inModel)
{
	mElemBaseModel = inModel;
}

LModelObject* CHTMLView::GetFormElemBaseModel(void)
{
	LModelObject* theBase;

	if (mElemBaseModel != NULL)
		theBase = mElemBaseModel;
	else
		theBase = LModelObject::GetDefaultModel();

	return theBase;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Reset scroll mode to default (LO_SCROLL_AUTO). Use to transition from LO_SCROLL_NEVER.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CHTMLView::ResetScrollMode(Boolean inRefresh)
{
	mScrollMode = LO_SCROLL_YES;
	SetScrollMode(mScrollMode, inRefresh);
	mScrollMode = mDefaultScrollMode;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Reset scroll mode to default (LO_SCROLL_AUTO). This really resets to
//	default.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 97.12.19 pchen. Add method to *REALLY* reset scroll mode to default scroll mode.
void CHTMLView::ResetToDefaultScrollMode()
{
	SetScrollMode(mDefaultScrollMode);
	mScrollMode = mDefaultScrollMode;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LO_SCROLL_NEVER disallows changing the scroll mode until ResetScrollMode() called.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::SetScrollMode(
	Int8 					inScrollMode,
	Boolean					inRefresh)
{
	if (mScrollMode != LO_SCROLL_NEVER) mScrollMode = inScrollMode;
	if (mScroller != NULL)
		{
		if ((mScrollMode == LO_SCROLL_NEVER) || (mScrollMode == LO_SCROLL_NO) || (mScrollMode == LO_SCROLL_AUTO))
			mScroller->ShowScrollbars(false, false);
		else
			mScroller->ShowScrollbars(true, true);

		// FIX ME!!! if this is never used, take it out
		if ( inRefresh )
			;// ¥¥ FIX ME
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CHTMLView::GetScrollMode(void) const
{
	return mScrollMode;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::SpendTime(const EventRecord& /* inMacEvent */)
{
//	// Pagination
//	if (fLastLedge != NO_LEDGE && fNextLedgeUpdate < TickCount())
//		DoSetDocDimension( fLastLedge, fLastWidthDelta, fLastHeightDelta );
//
	// Refresh timer
	if (mTimerURLString &&
		(mTimerURLFireTime < LMGetTicks()) &&
		IsEnabled() &&
		( XP_IsContextBusy(*mContext) == false) &&
		CFrontApp::GetApplication() && CFrontApp::GetApplication()->HasProperlyStartedUp())
	{
		URL_Struct * request = NET_CreateURLStruct (mTimerURLString, NET_NORMAL_RELOAD);
		ClearTimerURL();	// ...frees mTimerURLString, so must do this _after_ |NET_CreateURLStruct|.
		if (request)
		{
			request->referer = XP_STRDUP( mContext->GetCurrentURL() );
			mContext->SwitchLoadURL( request, FO_CACHE_AND_PRESENT );
		}
	}
}
#pragma mark --- I18N SUPPORT ---

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Int16 CHTMLView::DefaultCSIDForNewWindow(void)
{
	Int16 csid = 0;
	if (mContext)
	{
		csid = mContext->GetDocCSID();
		if(0 == csid)
			csid = mContext->GetDefaultCSID();
	}
	return csid;
}
Int16 CHTMLView::GetWinCSID(void) const
{
	if (mContext)
		return mContext->GetWinCSID();
	else
		return 0;
}

void CHTMLView::SetFontInfo()
{
     Boolean found;
     found = CPrefs::GetFont( GetWinCSID(), &mCharSet );
}
  
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- NOTIFICATION RESPONSE ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Mocha submit callback
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void MochaFormSubmitCallback(MWContext* pContext,
							 LO_Element* lo_element,
							 int32 lType,
							 void* whatever,
							 ETEventStatus status);

void MochaFormSubmitCallback(MWContext* pContext,
							 LO_Element* lo_element,
							 int32 /* lType */,
							 void* /* whatever */,
							 ETEventStatus status)
{
	if (status == EVENT_OK)
	{
		// Call the LO module to figure out the form's context
		LO_FormSubmitData* submit = LO_SubmitForm(pContext, (LO_FormElementStruct*)lo_element);
		if (submit == NULL)
			return;
		URL_Struct* url =  NET_CreateURLStruct((char *)submit->action, NET_DONT_RELOAD);
		CBrowserContext* context = ExtractBrowserContext(pContext);
		if (context)
		{
			History_entry* current = context->GetCurrentHistoryEntry();
			if (current && current->address)
				url->referer = XP_STRDUP(current->address);
			NET_AddLOSubmitDataToURLStruct(submit, url);
			context->SwitchLoadURL(url, FO_CACHE_AND_PRESENT);
		}
		LO_FreeSubmitData(submit);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Mocha image form submit callback
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

struct ImageFormSubmitData
{
	LO_ImageStruct*	lo_image;
	int32			x;
	int32			y;
};

void MochaImageFormSubmitCallback(MWContext* pContext,
								  LO_Element* lo_element,
								  int32 lType,
								  void* whatever,
								  ETEventStatus status);

void MochaImageFormSubmitCallback(MWContext* pContext,
								  LO_Element* /* lo_element */,
								  int32 /* lType */,
								  void* whatever,
								  ETEventStatus status)
{
	if (status == EVENT_OK)
	{
		LO_FormSubmitData *theSubmit = NULL;
		ImageFormSubmitData *data = reinterpret_cast<ImageFormSubmitData*>(whatever);
		try
			{
			theSubmit = LO_SubmitImageForm(pContext, data->lo_image, data->x, data->y);
//			ThrowIfNULL_(theSubmit);
			
			// 97-06-07 pkc -- NULL is a valid return value from LO_SubmitImageForm
			if (theSubmit)
				{
				URL_Struct* theURL = NET_CreateURLStruct((char*)theSubmit->action, NET_DONT_RELOAD); 
				ThrowIfNULL_(theURL);
				
				CBrowserContext* theContext = ExtractBrowserContext(pContext);
				
				if (theContext)
					{
					cstring theCurrentURL = theContext->GetCurrentURL();
					if (theCurrentURL.length() > 0)
						theURL->referer = XP_STRDUP(theCurrentURL);

					if (NET_AddLOSubmitDataToURLStruct(theSubmit, theURL))
						CURLDispatcher::DispatchURL(theURL, theContext, true);

					LO_FreeSubmitData(theSubmit);
					}
				}
			}
		catch (...)
			{
			LO_FreeSubmitData(theSubmit);
			throw;			
			}
	}
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::ListenToMessage(
	MessageT				inMessage,
	void*					ioParam)
{
	switch (inMessage)
		{
		case msg_NSCFinishedLayout:
			NoteFinishedLayout();
			break;
	
		case msg_NSCAllConnectionsComplete:
			NoteAllConnectionsComplete();
			
			/* update any commands which might change based on allConnectionsComplete */
			LCommander::SetUpdateCommandStatus(true);
			break;
		
		case msg_NSCPEmptyRepagination:
			NoteEmptyRepagination();
			break;	
			
		case msg_NSCPAboutToRepaginate:
			NoteStartRepagination();
			break;
			
		case msg_NSCConfirmLoadNewURL:
			NoteConfirmLoadNewURL(*(Boolean*)ioParam);
			break;
		
		case msg_NSCStartLoadURL:
			NoteStartLoadURL();
			break;
			
		case msg_NSCProgressMessageChanged:
			// Update Panes to enable the stop button
			if (!mStopEnablerHackExecuted &&
					mLoadingURL &&
					GetContext() == GetContext()->GetTopContext())
			{
				CPaneEnabler::UpdatePanes();
				mStopEnablerHackExecuted = true;
			}
			break;			
			
		case msg_NSCGridContextPreDispose:
			NoteGridContextPreDispose(*(Boolean*)ioParam);
			break;
			
		case msg_NSCGridContextDisposed:
			NoteGridContextDisposed();
			break;

		// FORMS
		case msg_SubmitButton:		// submission
		case msg_SubmitText:
		{
			void * formID;

			StTempFormBlur	tempBlur;	// see mforms.h for an explanation

			if (inMessage == msg_SubmitButton)
			{
				LPane* thePane = (LPane*)ioParam;
				
				LControl* theControl = dynamic_cast<LControl*>(thePane);
				ThrowIfNil_(theControl);
				
				formID = (void*) theControl->GetUserCon();
			}
			else
				formID = ((CFormLittleText*)ioParam)->GetLayoutForm();
				
/*			if ( true ) // LM_SendOnSubmit(*mContext, (LO_Element*) formID)
			{
				// Call the LO module to figure out the form's context
				submit = LO_SubmitForm(*mContext,( LO_FormElementStruct *)formID);
				if (submit == NULL)
					return;
				URL_Struct * url =  NET_CreateURLStruct((char *)submit->action, NET_DONT_RELOAD);
				History_entry* current = mContext->GetCurrentHistoryEntry();
				if (current && current->address)
					url->referer = XP_STRDUP(current->address);
				NET_AddLOSubmitDataToURLStruct(submit, url);
				mContext->SwitchLoadURL(url, FO_CACHE_AND_PRESENT);
				LO_FreeSubmitData(submit);
			}
*/
			// ET_SendEvent now takes a JSEvent struct instead of an int type
			JSEvent* event = XP_NEW_ZAP(JSEvent);
			if (event)
			{
				event->type = EVENT_SUBMIT;
				// The code above will get executed in MochaFormSubmitCallback
				ET_SendEvent(*mContext,
							 (LO_Element*)formID,
							 event,
							 MochaFormSubmitCallback,
							 NULL);
			}
		}
		break;
		
		case msg_ResetButton:		// Reset the form. ioParam is LStdButton
			{
				LPane* thePane = (LPane*)ioParam;
				
				LControl* theControl = dynamic_cast<LControl*>(thePane);
				ThrowIfNil_(theControl);
				
				LO_ResetForm(*mContext, (LO_FormElementStruct*) theControl->GetUserCon());
			}
			
			break;
		case msg_ControlClicked:	// Click on the radio button. Change da others
			{
				LPane* thePane = (LPane*)ioParam;
				
				LControl* theControl = dynamic_cast<LControl*>(thePane);
				ThrowIfNil_(theControl);
				
				LO_FormRadioSet(*mContext, (LO_FormElementStruct*) theControl->GetUserCon());
			}
			break;
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::NoteFinishedLayout(void)
{
	FlushPendingDocResize();

		/*
			Ugly hack: since `asynchronous' layout might have slightly overwritten the
			scrollbars, get just the scrollbars to redraw.
		*/

	if ( mScroller )
		mScroller->RefreshSelf();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::NoteAllConnectionsComplete(void)
{
//	StopRepeating();

		// Look to see if my context, or any context enclosing my context, needs to be repaginated.
	CBrowserContext* contextToRepaginate = 0;
	if ( mContext )
		for ( MWContext* ctx = *mContext; ctx; ctx = ctx->grid_parent )
			{
				CBrowserContext* browserContext = ExtractBrowserContext(ctx);
				if ( browserContext && browserContext->IsRepagintaitonPending() )
					contextToRepaginate = browserContext;
			}

		/*
			Try to repaginate the most enclosing context that needs it.  If some busy child
			stops the repagination, no big deal... that childs |NoteAllConnectionsComplete|
			will try again.
		*/

	if ( contextToRepaginate )
		contextToRepaginate->Repaginate();
	else
		ClearDeferredImageQueue();

	//Enable();
	CPaneEnabler::UpdatePanes();
	
	mLoadingURL = false;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::NoteStartRepagination(void)
{
	SPoint32 theScrollPosition;
	GetScrollPosition(theScrollPosition);
	mContext->RememberHistoryPosition(theScrollPosition.h, theScrollPosition.v);
	
// FIX ME!!! may want to reset the size flush timer here
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::NoteEmptyRepagination(void)
{
	// Adjust scroll bars
	
	AdjustScrollBars();	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::NoteConfirmLoadNewURL(Boolean& ioCanLoad)
{
//	if ( HasDownloads( *mContext ) )
//		if (!ErrorManager::PlainConfirm((const char*) GetCString( ABORT_CURR_DOWNLOAD ), nil, nil, nil ))
//			return;

	ioCanLoad = true;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::NoteStartLoadURL(void)
{
	//Disable();
// FIX ME!!! we should disable the view.  it will be enabled through all connection complete

	ClearTimerURL();	// jrm 97/08/22

	SPoint32 theScrollPosition;
	GetScrollPosition(theScrollPosition);
	mContext->RememberHistoryPosition(theScrollPosition.h, theScrollPosition.v);
	mLoadingURL = true;
	mStopEnablerHackExecuted = false;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::NoteGridContextPreDispose(Boolean inSavingHistory)
{
	if (inSavingHistory)
		{
		SPoint32 theScrollPosition;
		GetScrollPosition(theScrollPosition);
		mContext->RememberHistoryPosition(theScrollPosition.h, theScrollPosition.v);
		}
		
	// our grid context is going to be orphaned by it's super context
	// which is our clue that we aren't supposed to be here anymore.
	// Deleting outself releases our shared interest in the context.
	//
	// FIX ME!!! This needs to change.  The assumption here is that the
	// superview of the HTML view is a scroller and was created at the
	// same time as this.  This will need to go away when the scroller
	// dependency is removed.
	delete mSuperView;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::NoteGridContextDisposed(void)
{
	if (mContext->CountGridChildren() == 0)
		SetScrollMode(mDefaultScrollMode, true);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// replaces the old fCanLoad Member
void CHTMLView::EnableSelf(void)
{
	// for the click code
	mOldPoint.h = mOldPoint.v = -1;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::DisableSelf(void)
{
	mOldPoint.h = mOldPoint.v = -1;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- DRAWING AND UPDATING ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::ResetBackgroundColor() const
{
	UGraphics::SetIfBkColor(mBackgroundColor);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	ContextMenuPopupsEnabled
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// determine whether context-sensitive popup menus are enabled by querying
// the parent browser window

Boolean CHTMLView::ContextMenuPopupsEnabled (void)
{
	CBrowserWindow	*browserWindow = dynamic_cast<CBrowserWindow *>(CViewUtils::GetTopMostSuperView (this));
	// 8-13-97 context menus disabled when commands are disabled unless the control key is down.
	// this is mac specific and may go away when we have an xp way to disable context menus from javascript.
	return browserWindow ? browserWindow->AllowSubviewPopups() && !browserWindow->IsHTMLHelp() && (!browserWindow->CommandsAreDisabled() || CApplicationEventAttachment::CurrentEventHasModifiers(controlKey)) : true;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	GetCurrentPort
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CGrafPtr CHTMLView::GetCurrentPort(Point& outPortOrigin)
{
	if ((mCurrentDrawable == NULL) || (mCurrentDrawable == mOnscreenDrawable)) {
		outPortOrigin = mPortOrigin;
		return (CGrafPtr) GetMacPort();
	}
	else {
		outPortOrigin.h = outPortOrigin.v = 0;
		return mCurrentDrawable->GetDrawableOffscreen();
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	FocusDraw
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CHTMLView::FocusDraw(LPane* /*inSubPane*/)
{
	Boolean bFocused = true;
	GWorldPtr gworld = NULL;

	// I don't really want to write my own FocusDraw, but PowerPlant has this nasty
	// habit of setting the origin and clip on me...
	bFocused = (mRevealedRect.left < mRevealedRect.right);
	
	// Skip if already in focus
	if (this != sInFocusView)
		{
		// Always set PowerPlant's origin and clip for the onscreen port.
		if (LView::EstablishPort())
			{
			// Set up local coordinate system
			::SetOrigin(mPortOrigin.h, mPortOrigin.v);
		
			// Clip to revealed area of View
			Rect clippingRect = mRevealedRect;
			
			PortToLocalPoint( topLeft(clippingRect) );
			PortToLocalPoint( botRight(clippingRect) );
			
			// if we're focussed then we need to do some extra stuff
			if ( bFocused && IsFocusedFrame() )
				{
				::InsetRect( &clippingRect, FocusBox_Size, FocusBox_Size );
				}

			::ClipRect( &clippingRect );
			}
			
		
		// Set our current Mac Port	- could be either onscreen or offscreen
		if (EstablishPort())
			{
			// If the current drawable is not us, don't screw with the origin and clip.
			// The drawable is set to nil during normal operation so that's assumed to
			// also be us.
			if ( ( mCurrentDrawable != NULL ) && ( mCurrentDrawable != mOnscreenDrawable ) )
				{
				// offscreen
				Rect theFrame;
				CalcLocalFrameRect(theFrame);

				::SetOrigin(theFrame.left, theFrame.top);
				}
			
			// Cache current Focus
			sInFocusView = this;
			
			// be sure to reset the background color
			if ( bFocused && ( mRevealedRect.left < mRevealedRect.right ) )
				{
				ResetBackgroundColor();
				}
			}
		else
			{
			SignalPStr_("\pFocus View with no GrafPort");
			}
		}
	
	// if we have a current drawable, then we need to do magic with the clip
	if ( mCurrentDrawable != NULL )
		{			
		if ( mCurrentDrawable->HasClipChanged() )
			{
			RgnHandle clip;
			
			//
			// Warning: If the drawable is our offscreen, then we need to make sure
			// to move the clip into the correct coordinate space. The problem here is that
			// the drawable has no way of knowing what its coordinate space is. To make 
			// matters worse, Drawables are shared across multiple HTMLViews.
			//
			// We could simplify this by keeping one back buffer for the browser as a whole
			// and then instantiating a COffscreenDrawable per CHTMLView. That way the HTMLView
			// could notify the offscreen whenever it's origin changes. It could then offset
			// its clip correctly whenever a new one is set.
			clip = (RgnHandle) mCurrentDrawable->GetLayerClip();
			if ( mCurrentDrawable == mOffscreenDrawable )
				{
				Rect theFrame;
				
				CalcLocalFrameRect(theFrame);
				
				::OffsetRgn ( clip, theFrame.left, theFrame.top );
				::SetClip ( clip );
				::OffsetRgn ( clip, -theFrame.left, -theFrame.top );
				}
			else
				{
				::SetClip ( clip );
				}
			}
		}
	
	return bFocused;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	EstablishPort
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
Boolean
CHTMLView::EstablishPort()
{
	Boolean portSet = false;
	GWorldPtr gworld = NULL;
	
	// if the current drawable, is an offscreen one, be sure to set it
	if ( mCurrentDrawable != NULL )
		{
		gworld = mCurrentDrawable->GetDrawableOffscreen();
		}
	
	if ( gworld != NULL )
		{
		portSet = true;
		if ( UQDGlobals::GetCurrentPort() != (GrafPtr) mGWorld )
			{
			SetGWorld ( gworld, NULL );
			mOffscreenDrawable->mClipChanged = true;
			}
		}
	else
		{
		// make sure to restore the main device
		SetGDevice ( GetMainDevice() );
		portSet = LView::EstablishPort();
		}
			
	return portSet;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawSelf
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::DrawSelf(void)
{
	if (Memory_MemoryIsLow() || !mContext)
		return;
		// A CHTMLView can have a nil mContext, but if it does, it doesn't draw.
	
	// update the image display context if we need to. it would be nice to reload the images
	// on the current page, but we cuurently cannot doing so without other side effects
	// such as losing form data...
	VerifyDisplayContextColorSpace ( *mContext );
	
// this is the new (layered) drawing.  It flashes WAY to much.
// FIX ME!!! clean up

	if (GetRepaginate())
	{
		GetContext()->Repaginate(NET_CACHE_ONLY_RELOAD);
		SetRepaginate(false);
	}

	// For layers all drawing is done through the compositor 
	// BUGBUG LAYERS The assumption here is that the cutout region
	// (including plugins) will be removed from the composited
	// area...this still needs to be done.

//	if ( sNoTextUpdateLock )
//		return;
		
	// Get the update region
	StRegion theImageUpdateRgn(mUpdateRgnH);
	
	if (!mContext || !mContext->GetCurrentHistoryEntry())
	{
		ClearBackground();
	}
	else
	{
		/* Convert to frame coordinates */
		SPoint32 theFrameLocation;
		GetFrameLocation(theFrameLocation);
		::OffsetRgn(theImageUpdateRgn, -theFrameLocation.h, -theFrameLocation.v);

		if (mCompositor && CL_GetCompositorEnabled(*mCompositor))
			CL_RefreshWindowRegion(*mCompositor, (FE_Region)static_cast<RgnHandle>(theImageUpdateRgn));
		else
			DrawBackground((*static_cast<RgnHandle>(theImageUpdateRgn))->rgnBBox);	

		// 07-06-11 pkc -- Update grid edges only if mContext isn't busy which means that
		// layout isn't running, i.e. calling FE_DisplayEdge directly
		/* 18.Jun.97 drm -- removed the check so we update grid edges even when layout
		   is already doing that.  While layout is creating a new page, the last redraw
		   that goes through tends to happen while the context is busy.  This check
		   causes frame edges to not be redrawn, which leaves a bevel line from the
		   enclosing CBevelView running across the end of the frame edge.
		*/
//		if (!XP_IsContextBusy(*mContext))
			DrawGridEdges(theImageUpdateRgn);	
	}
	
	if (IsFocusedFrame())
		DrawFrameFocus();
	ExecuteAttachments(CTargetFramer::msg_DrawSelfDone, this);
	

/* this is the old drawing code
	mCalcDontDraw = false;
	
	Rect theFrame;
	CalcLocalFrameRect(theFrame);
	RgnHandle theLocalUpdateRgn = GetLocalUpdateRgn();

	Rect theLocalUpdateFrame = (*theLocalUpdateRgn)->rgnBBox;
	::SectRect(&theFrame, &theLocalUpdateFrame, &theLocalUpdateFrame);

	if (!mHasGridCells)
		DrawBackground(theLocalUpdateFrame);

	SPoint32 		itl, ibr;
	Point			tl, br;
	tl = topLeft( theLocalUpdateFrame );
	br = botRight( theLocalUpdateFrame );
	LocalToImagePoint( tl, itl );
	LocalToImagePoint( br, ibr );
	
	LO_RefreshArea(*mContext, itl.h, itl.v, ibr.h - itl.h, ibr.v - itl.v );

	if (IsFocusedFrame())
		DrawFrameFocus();

	::DisposeRgn(theLocalUpdateRgn);
	mCalcDontDraw = true;
*/
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	DrawFrameFocus
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::DrawFrameFocus(void)
{
	StRegion theFocusMask;
	CalcFrameFocusMask(theFocusMask);
	
	Point	theLocalOffset = { 0, 0 };
	PortToLocalPoint(theLocalOffset);
	::OffsetRgn(theFocusMask, theLocalOffset.h, theLocalOffset.v);

	StClipRgnState theClipSaver(theFocusMask);
	StColorPenState	savePen;

	::PenPat(&UQDGlobals::GetQDGlobals()->black);
	::PenMode(srcCopy);

	RGBColor theHiliteColor;
	LMGetHiliteRGB(&theHiliteColor);
	::RGBForeColor(&theHiliteColor);

	::PaintRgn(theFocusMask);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CalcFrameFocusMask
//
//	This region is in port coordinates
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::CalcFrameFocusMask(RgnHandle outFocusMask)
{
	::SetEmptyRgn(outFocusMask);

	Rect theFrame;
	CalcPortFrameRect(theFrame);
	::SectRect(&theFrame, &mRevealedRect, &theFrame);

	::RectRgn(outFocusMask, &theFrame);
	StRegion theSubFocus(theFrame);
	
	::InsetRgn(theSubFocus, FocusBox_Size, FocusBox_Size);
	::DiffRgn(outFocusMask, theSubFocus, outFocusMask);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	InvalFocusArea
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::InvalFocusArea(void)
{
	if (mShowFocus)
		{
		StRegion theFocusMask;
		CalcFrameFocusMask(theFocusMask);
		InvalPortRgn(theFocusMask);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	AdjustScrollBars
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// FIX ME!!! this needs to be removed when the mScroller is removed
void CHTMLView::AdjustScrollBars()
{
	if ((mScroller == NULL) || (GetScrollMode() != LO_SCROLL_AUTO))
		return;

	mScroller->AdjustScrollBars();

/*
	SDimension16 theFrameSize;
	GetFrameSize(theFrameSize);
	
	SDimension32 theImageSize;
	GetImageSize(theImageSize);
	
	// If we are only showing scrollers when needed, turn them on if we have overgrown

	mScroller->ShowScrollbars(
								theImageSize.width  > theFrameSize.width,
								theImageSize.height > theFrameSize.height);
*/
}

void
CHTMLView::RegisterCallBackCalls()
{
	PREF_RegisterCallback("intl", PrefUpdateCallback, (void *)this);
	// At some point we should decide if it makes more sense to register a single
	// "browser" call back.
	PREF_RegisterCallback(	"browser.underline_anchors",
							PrefUpdateCallback, (void *)this);
	PREF_RegisterCallback(	"browser.background_color",
								PrefUpdateCallback, (void *)this);
	PREF_RegisterCallback(	"browser.use_document_fonts",
								PrefUpdateCallback, (void *)this);
	PREF_RegisterCallback(	"browser.foreground_color",
								PrefUpdateCallback, (void *)this);
	PREF_RegisterCallback(	"browser.anchor_color",
								PrefUpdateCallback, (void *)this);
	PREF_RegisterCallback(	"browser.visited_color",
								PrefUpdateCallback, (void *)this);
	PREF_RegisterCallback(	"browser.use_document_colors",
								PrefUpdateCallback, (void *)this);
	PREF_RegisterCallback(	"general.always_load_images",
								PrefInvalidateCachedPreference, (void *)this);
}

void
CHTMLView::UnregisterCallBackCalls()
{
	PREF_UnregisterCallback("intl", PrefUpdateCallback, (void *)this);
	PREF_UnregisterCallback(	"browser.underline_anchors",
								PrefUpdateCallback, (void *)this);
	PREF_UnregisterCallback(	"browser.background_color",
								PrefUpdateCallback, (void *)this);
	PREF_UnregisterCallback(	"browser.use_document_fonts",
								PrefUpdateCallback, (void *)this);
	PREF_UnregisterCallback(	"browser.foreground_color",
								PrefUpdateCallback, (void *)this);
	PREF_UnregisterCallback(	"browser.anchor_color",
								PrefUpdateCallback, (void *)this);
	PREF_UnregisterCallback(	"browser.visited_color",
								PrefUpdateCallback, (void *)this);
	PREF_UnregisterCallback(	"browser.use_document_colors",
								PrefUpdateCallback, (void *)this);
	PREF_UnregisterCallback(	"general.always_load_images",
								PrefInvalidateCachedPreference, (void *)this);												
}

int
CHTMLView::PrefUpdateCallback(const char * /*inPrefString */, void *inCHTMLView)
{
	CHTMLView	*theView = (CHTMLView *)inCHTMLView;
	theView = dynamic_cast<CHTMLView*>(theView);
	XP_ASSERT(theView);
	XP_ASSERT(!theView->GetSuperHTMLView());
	if (theView &&
		!theView->GetRepaginate() &&	// No need to do it again if we have been here before.
		theView->IsRootHTMLView())
	{
		theView->SetRepaginate(true);
		Rect	paneRect;
		theView->CalcPortFrameRect(paneRect);
		theView->InvalPortRect(&paneRect);
	}
	return 0;	// You don't even want to know my opinion of this!
}

int
CHTMLView::PrefInvalidateCachedPreference(const char *inPrefString, void * /*inCHTMLView */)
{
	int	returnValue = 0;
	 
	if (!XP_STRCMP(inPrefString, "general.always_load_images")) {
		XP_Bool	prefValue;
		returnValue = PREF_GetBoolPref(inPrefString, &prefValue);
		if (returnValue != PREF_OK && returnValue != PREF_NOERROR)
			sCachedAlwaysLoadImages = true;
		else
			sCachedAlwaysLoadImages = (Boolean)prefValue;
	}
	
	return returnValue;
}

// 97-06-11 pkc -- Code to handle redrawing grid edges
void CHTMLView::DrawGridEdges(RgnHandle inRgnHandle)
{
	// quick check -- if mGridEdgeList is empty, no grid edges to draw
	if(!mGridEdgeList.empty())
	{
		// set mDontAddGridEdgeToList to true so that that DisplayEdge does not
		// to add this LO_EdgeStruct* to mGridEdgeList
		mDontAddGridEdgeToList = true;
		// Check each grid edge to see if it intersects the update region
		vector<LO_EdgeStruct*>::iterator iter = mGridEdgeList.begin();
		while (iter != mGridEdgeList.end())
		{
			LO_EdgeStruct* edge = *iter;
			Rect edgeFrame;
			if (CalcElementPosition((LO_Element*)edge, edgeFrame))
			{
				// grid edge is visible
				if(::RectInRgn(&edgeFrame, inRgnHandle))
				{
					// grid edge is in update region, draw it
					DisplayEdge(NULL, edge);
				}
			}
			++iter;
		}
		// set mDontAddGridEdgeToList to false
		mDontAddGridEdgeToList = false;
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- LAYER DISPATCH ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::SetLayerOrigin(
	Int32 					inX,
	Int32 					inY)
{
	mLayerOrigin.h = inX;
	mLayerOrigin.v = inY;
}
	
void CHTMLView::GetLayerOrigin(
	Int32*					outX,
	Int32*					outY)
{
	/*
	 * certain things like images get the current layer origin from the current hyperview.
	 * If they can learn how to get this from the current drawable (through the doc context
	 * then this hack can go away
	 */
	if ( mCurrentDrawable != NULL && mCurrentDrawable != mOnscreenDrawable )
		{
		mCurrentDrawable->GetLayerOrigin ( outX, outY );
		}
	else
		{
		*outX = mLayerOrigin.h;
		*outY = mLayerOrigin.v;
		}
}
	
void CHTMLView::SetLayerClip(
	FE_Region 				inRegion)
{

	// do we need to restore a magic region?
	if ( inRegion == NULL )
		{
		mLayerClip = NULL;
		
		if (!::EmptyRgn(mSaveLayerClip))
			{
			mClipChanged = true;
			::CopyRgn ( mSaveLayerClip, mClipRgn );
			::SetEmptyRgn(mSaveLayerClip);
			}
		}
	else
		{
		Point portOrigin, scrollOffset;
		SPoint32 frameLocation;

		// do we need to save a magic region?
		if ( mLayerClip == NULL )
			{
			::GetClip(mSaveLayerClip);
			mLayerClip = FE_GetMDRegion(inRegion);
			}
		
		// now, copy this region to our own copy and move it to the correct coordinate
		// space
		CopyRgn ( FE_GetMDRegion(inRegion), mClipRgn );
	
		// The region passed in is in frame coordinates. To use it,
		// we need to convert it to local coordinates. The scrollOffset
		// represents how much we need to offset the region for this 
		// transformation.
		GetPortOrigin(portOrigin);
		GetFrameLocation(frameLocation);
		
		scrollOffset.h = portOrigin.h + frameLocation.h;
		scrollOffset.v = portOrigin.v + frameLocation.v;
			
		::OffsetRgn(mClipRgn, scrollOffset.h, scrollOffset.v);
		
		mClipChanged = true;
		}

	// because we're lame and don't know how to test if the current
	// port is us, we save the port, and then focus ourselves. since
	// we marked the clip as having changed, it will always be set
	// How can I tell if I'm the current port?

	GDHandle saveGD;
	CGrafPtr savePort;
	
	GetGWorld ( &savePort, &saveGD );

	FocusDraw();
	
	SetGWorld ( savePort, saveGD );
}

void CHTMLView::CopyPixels(
	CDrawable*				inSrcDrawable,
	FE_Region				inCopyRgn)
{
	GWorldPtr		srcGWorld;
	
	FocusDraw();

	srcGWorld = inSrcDrawable->GetDrawableOffscreen();
	if ( srcGWorld != NULL )
		{
		Rect srcRect;
		Rect dstRect;
		
		GDHandle gdh;
		CGrafPtr port;
		
		GetGWorld ( &port, &gdh );
		SetGWorld ( srcGWorld, NULL );
		
		SetOrigin ( 0, 0 );
		
		SetGWorld ( port, gdh );
		
		StColorPenState save;
		
		StColorPenState::Normalize();
		
		/* we use the bounding rectangle for the copy rgn as the src/dst rect */
		/* for the copy. however, we need to make sure that the onscreen rect is */
		/* in the correct coordinate space */
		srcRect = (*(RgnHandle) inCopyRgn)->rgnBBox;
		dstRect = srcRect;
				
		Point portOrigin, scrollOffset;
		SPoint32 frameLocation;
		
		// The region passed in is in frame coordinates. To use it,
		// we need to convert it to local coordinates. The scrollOffset
		// represents how much we need to offset the region for this 
		// transformation.
		GetPortOrigin(portOrigin);
		GetFrameLocation(frameLocation);
		
		scrollOffset.h = portOrigin.h + frameLocation.h;
		scrollOffset.v = portOrigin.v + frameLocation.v;
				
		::OffsetRgn(FE_GetMDRegion(inCopyRgn), scrollOffset.h, scrollOffset.v );
		::OffsetRect ( &dstRect, scrollOffset.h, scrollOffset.v );
		
		CopyBits ( &((GrafPtr) srcGWorld)->portBits, &qd.thePort->portBits, &srcRect,
			&dstRect, srcCopy, FE_GetMDRegion(inCopyRgn) );
		
		::OffsetRgn(FE_GetMDRegion(inCopyRgn), -scrollOffset.h, -scrollOffset.v);
		}
}

// Dispatcher for events arriving from layers. These events may have originated in
// the front end and sent to layers, or they may have been synthesized.
PRBool CHTMLView::HandleLayerEvent(
	CL_Layer*				inLayer,
	CL_Event*				inEvent)
{
	CStLayerOriginSetter makeOriginInLayerOrigin(this, inLayer);
	fe_EventStruct *fe_event = (fe_EventStruct *)inEvent->fe_event;
	
	SPoint32 theLayerPoint;
	theLayerPoint.h = inEvent->x;
	theLayerPoint.v = inEvent->y;
	
	if (!fe_event)
		// Fill in FE event if the event was synthesized in the backend
		{
		EventRecord event;
		Point whereLocal, wherePort, whereGlobal;
		
		ImageToLocalPoint(theLayerPoint, whereLocal);
		wherePort = whereLocal;
		LocalToPortPoint(wherePort);
		whereGlobal = wherePort;
		PortToLocalPoint(whereGlobal);
		
		event.when = ::TickCount();
		event.where = whereGlobal;
		event.modifiers = ((inEvent->modifiers & EVENT_SHIFT_MASK) ? shiftKey : 0) |
							((inEvent->modifiers & EVENT_CONTROL_MASK) ? controlKey : 0) |
							((inEvent->modifiers & EVENT_ALT_MASK) ? optionKey : 0) |
							((inEvent->modifiers & EVENT_META_MASK) ? cmdKey : 0);
		
		switch (inEvent->type)
			{
			case CL_EVENT_MOUSE_BUTTON_DOWN:
			case CL_EVENT_MOUSE_BUTTON_MULTI_CLICK:
				SMouseDownEvent mouseDownEvent;
				LWindow *window = LWindow::FetchWindowObject(GetMacPort());
				
				event.what = mouseDown;
				// event.message is undefined
				mouseDownEvent.wherePort = wherePort;
				mouseDownEvent.whereLocal = whereLocal;
				mouseDownEvent.macEvent = event;
				mouseDownEvent.delaySelect = (window != nil) ? (!UDesktop::WindowIsSelected(window) && window->HasAttribute(windAttr_DelaySelect)) : false;
				fe_event->event.mouseDownEvent = mouseDownEvent;
				break;
			case CL_EVENT_MOUSE_MOVE:
				event.what = nullEvent;
				event.message = mouseMovedMessage;
				fe_event->event.macEvent = event;
				break;
			case CL_EVENT_KEY_DOWN:
				event.what = keyDown;
				event.message = inEvent->which;
				fe_event->event.macEvent = event;
				break;
			default:
				return PR_TRUE; // ignore the event
			}
		}

/*
	// Make sure this event came from the front-end. 
	// i.e. ignore synthesized events
		return PR_TRUE;
*/
	
	// Call the per-layer event handlers
	switch (inEvent->type)
		{
		case CL_EVENT_MOUSE_BUTTON_DOWN:
		case CL_EVENT_MOUSE_BUTTON_MULTI_CLICK:
			{
			//SMouseDownEvent* mouseDown = (SMouseDownEvent *)fe_event->event;
			//ClickSelfLayer(*mouseDown, inLayer, theLayerPoint);
			SMouseDownEvent mouseDown = fe_event->event.mouseDownEvent;
			ClickSelfLayer(mouseDown, inLayer, theLayerPoint);
			}
			break;
			
		case CL_EVENT_MOUSE_MOVE:
			{
			Point portPt = fe_event->portPoint;
			//EventRecord *macEvent = (EventRecord *)fe_event->event;
			//AdjustCursorSelfForLayer(portPt, *macEvent, inLayer, theLayerPoint);
			// modified for new fe_EventStruct typedef 1997-02-25 mjc
			EventRecord macEvent = fe_event->event.macEvent;
			AdjustCursorSelfForLayer(portPt, macEvent, inLayer, theLayerPoint);
			}
			break;
		case CL_EVENT_KEY_DOWN:
			{
			EventRecord macEvent = fe_event->event.macEvent;
			HandleKeyPressLayer(macEvent, inLayer, theLayerPoint);
			break;
			}
		case CL_EVENT_KEY_UP:
		case CL_EVENT_MOUSE_BUTTON_UP:
		case CL_EVENT_KEY_FOCUS_GAINED:
		case CL_EVENT_KEY_FOCUS_LOST:
			// Nothing to do, but grab the event
			break;
		default:
			// Pass the event through
			return PR_FALSE;
	}
	
	return PR_TRUE;			

}

PRBool CHTMLView::HandleEmbedEvent(	
	LO_EmbedStruct*			inEmbed, 
	CL_Event*				inEvent)
{
	NPEmbeddedApp* app = (NPEmbeddedApp*) inEmbed->FE_Data;
	if (app && app->fe_data)
	{
		CPluginView* view = (CPluginView*) app->fe_data;
		return (PRBool)view->HandleEmbedEvent(inEvent);
	}
	return PR_FALSE;
}
	
void CHTMLView::ScrollImageBy(
	Int32					inLeftDelta,
	Int32					inTopDelta,
	Boolean 				inRefresh)
{
	if ( ( inLeftDelta == 0 ) && ( inTopDelta == 0 ) )
		return;
		
	// Get the image coordinates of the frame origin
	
	Rect theFrame;
	CalcLocalFrameRect(theFrame);

	SPoint32	imageTopLeft;
	LocalToImagePoint(topLeft(theFrame), imageTopLeft);
	imageTopLeft.h += inLeftDelta;
	imageTopLeft.v += inTopDelta;
	
	if (mCompositor != NULL)
		CL_ScrollCompositorWindow(*mCompositor, imageTopLeft.h, imageTopLeft.v);			

// FIX ME!!! this seems like it might be a bug.  The image can be pinned and 
// not scroll as much as the request.  It seems like the commpositor would be out of sync.

	// Let PowerPlant do the rest 
	Boolean mayAutoScrollLeavingTurds = inRefresh && IsTarget(); //&& mDropRow;
		// Turds will be left if we call scrollbits in ScrollImageBy
	if (mayAutoScrollLeavingTurds)
	{
		// Notify the CTargetFramer (if any) to erase the frame hilite
		ExecuteAttachments(CTargetFramer::msg_ResigningTarget, this);
	}
	LView::ScrollImageBy(inLeftDelta, inTopDelta, inRefresh);
	if (mayAutoScrollLeavingTurds && FocusDraw())
	{
		// Turn hiliting back on
		// Notify the CTargetFramer to draw the border now.
		ExecuteAttachments(CTargetFramer::msg_BecomingTarget, this); // invert
	}
	// normally, the compositor will add forms control subpanes later, during
	// LPeriodical time.  if the subpanes are being displayed now for the first time,
	// they've never been added to this view, so we need to force the compositor to
	// add them now, while the update region is still fresh.
	if (mCompositor)
		CL_CompositeNow (*mCompositor);
}


// until flag is turned on...
extern "C" {
void LO_RelayoutOnResize(MWContext *context, int32 width, int32 height, int32 leftMargin, int32 topMargin);
}

// This method gets called when window changes size.
// Make sure we call mContext->Repaginate()
void CHTMLView::AdaptToSuperFrameSize(
	Int32					inSurrWidthDelta,
	Int32					inSurrHeightDelta,
	Boolean					inRefresh)
{
	UCursor::SetWatch();
	
	LView::AdaptToSuperFrameSize(inSurrWidthDelta, inSurrHeightDelta, inRefresh);

	if (IsRootHTMLView())
		{
		if ((mContext != NULL) && ((inSurrWidthDelta != 0) || (inSurrHeightDelta != 0)) && (!mContext->IsViewSourceContext()))
			{
			SDimension16	theFrameSize;
			Rect			theFrame;
			int32			leftMargin;
			int32			topMargin;
			
			GetFrameSize(theFrameSize);
			
			leftMargin = 8;
			topMargin = 8;
			
			CalcLocalFrameRect(theFrame);
			if ( theFrame.right - theFrame.left > 0 )
				{
				if (leftMargin > (theFrame.right / 2 ))
					leftMargin = MAX( theFrame.right / 2 - 50, 0);
				}
			if ( theFrame.bottom - theFrame.top > 0 )
				{
				if ( topMargin > (theFrame.bottom / 2 ) )
					topMargin = MAX(theFrame.bottom / 2 -50, 0);
				}
			
			// set the image size to the current frame size
			SDimension16 curFrameSize;
			GetFrameSize(curFrameSize);

			ResizeImageTo ( curFrameSize.width, curFrameSize.height, false );

			// If the vertical scrollbar might be shown,
			// tell layout that we already have it, so that it does
			// the correct wrapping of text.
			// If we do not do this, when vertical scrollbar only is
			// shown, some text might be hidden
			if (GetScrollMode() == LO_SCROLL_AUTO)
				theFrameSize.width -= 15;
			
			LO_RelayoutOnResize ( *mContext, theFrameSize.width, theFrameSize.height, leftMargin, topMargin );
			AdjustScrollBars();

			}
		}
	UCursor::SetArrow();
}

void CHTMLView::ResizeFrameBy(
	Int16 					inWidthDelta,
	Int16 					inHeightDelta,
	Boolean 				inRefresh)
{
	// are we on drugs?
	if ( ( inWidthDelta == 0 ) && ( inHeightDelta == 0 ) )
		return;

	SDimension16 theFrameSize;
	GetFrameSize(theFrameSize);
	theFrameSize.width += inWidthDelta;
	theFrameSize.height += inHeightDelta;

	// Let PowerPlant do the rest 
	LView::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	
	// We need to adjust the compositor before the inherited call in case
	// in refresh is true (immediate draw).
	if (mCompositor != NULL)
		CL_ResizeCompositorWindow(*mCompositor, theFrameSize.width, theFrameSize.height);			
	
	// We now call Repaginate in AdaptToSuperFrameSize
}

void CHTMLView::SetCurrentDrawable(
	CDrawable*			inDrawable)
{
	if ( inDrawable != NULL )
		{
		mCurrentDrawable = inDrawable;
		
		/* make sure this new drawable calls us */
		mCurrentDrawable->SetParent ( this );
		}
	else
		{
		mCurrentDrawable = NULL;
		}
					
	/* Our focus has most likely changed */
	OutOfFocus ( nil );
	FocusDraw();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- COMMANDER OVERRIDES ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void MochaFocusCallback(	MWContext * /* pContext */,
							LO_Element * /* lo_element */,
							int32 /* lType */,
							void * whatever,
							ETEventStatus status	)
{
	if (status == EVENT_OK && whatever)
	{
		CHTMLView* htmlView = reinterpret_cast<CHTMLView*>(whatever);
		htmlView->ClearInFocusCallAlready();
	}
}

void CHTMLView::PutOnDuty(LCommander*)
{
	if (IsFocusedFrame() && FocusDraw())
		DrawFrameFocus();
	// javascript onFocus callback - 1997-02-27 mjc
	if (mContext != NULL && !mInFocusCallAlready)
	{
		JSEvent* event = XP_NEW_ZAP(JSEvent);
		if (event)
		{
			event->type = EVENT_FOCUS;
			event->layer_id = LO_DOCUMENT_LAYER_ID;
			mInFocusCallAlready = true;
			ET_SendEvent( *mContext, NULL, event, MochaFocusCallback, this );
		}
	}
}

void CHTMLView::TakeOffDuty(void)
{
	if (FocusDraw())
		InvalFocusArea();
	// javascript onBlur callback  - 1997-02-27 mjc
	if (mContext != NULL && !mInFocusCallAlready)
	{
		JSEvent* event = XP_NEW_ZAP(JSEvent);
		if (event)
		{
			event->type = EVENT_BLUR;
			event->layer_id = LO_DOCUMENT_LAYER_ID;
			mInFocusCallAlready = true;
			ET_SendEvent( *mContext, NULL, event, MochaFocusCallback, this );
		}
	}
}

void CHTMLView::FindCommandStatus(CommandT inCommand,
									Boolean	&outEnabled,
									Boolean	&outUsesMark,
									Char16	&outMark,
									Str255	outName)
{
	outUsesMark = false;
	if (!mContext) // yes, this can happen.  It happened to me!
	{
		LTabGroup::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
		return;
	}
	
	CBrowserWindow	*browserWindow = dynamic_cast<CBrowserWindow *>(CViewUtils::GetTopMostSuperView(this));
	Boolean isRootDocInfo	= browserWindow ? browserWindow->IsRootDocInfo() : false;
	Boolean isHTMLHelp		= browserWindow ? browserWindow->IsHTMLHelp() : false;
	Boolean	isViewSource	= browserWindow ? browserWindow->IsViewSource() : false;
	
	// Disable commands which don't apply to restricted targets, at the request of javascript.
	// No special distinction needed for floaters here.
	if (browserWindow && 
		browserWindow->CommandsAreDisabled() &&
		(		
			inCommand == cmd_AddToBookmarks ||
			inCommand == cmd_ViewSource ||
			inCommand == cmd_DocumentInfo ||
			inCommand == cmd_Find ||
			inCommand == cmd_FindAgain ||
			inCommand == cmd_MailDocument ||
			inCommand == cmd_LoadImages ||
			inCommand == cmd_Reload ||
			inCommand == cmd_SaveAs))
	{
		outEnabled = false;
		return;
	}

	switch (inCommand)
	{			
		case cmd_LoadImages:
			if (mContext && !isViewSource)
			{
				outEnabled = !sCachedAlwaysLoadImages;
			}
			break;

		case cmd_Reload:
			{
				if (GetContext())
				{
					CBrowserContext* theTopContext = GetContext()->GetTopContext();
					if ((theTopContext != nil) && (theTopContext->GetCurrentHistoryEntry() != nil) && !XP_IsContextBusy(*theTopContext))
					{
						outEnabled = true;
						
						if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey) ||
							CApplicationEventAttachment::CurrentEventHasModifiers(shiftKey))
						{
							LString::CopyPStr(::GetPString(MENU_SUPER_RELOAD), outName);
						}
						else
						{
							LString::CopyPStr(::GetPString(MENU_RELOAD), outName);
						}
					}
				}
			}
			break;

		case cmd_SecurityInfo:
		{
			outEnabled = !UDesktop::FrontWindowIsModal();
			break;
		}
		case cmd_ViewSource:
		case cmd_DocumentInfo:
		{
			outEnabled = !isHTMLHelp;
			break;
		}
		case cmd_AddToBookmarks:
			outUsesMark = false;
			outEnabled = mContext && mContext->GetCurrentHistoryEntry() && !(isRootDocInfo || isHTMLHelp);
			break;
		case cmd_SaveAs:
		{
			if ((mContext->GetCurrentHistoryEntry() != nil) && !XP_IsContextBusy(*mContext))
			{
				outEnabled = !isHTMLHelp;
			}
			break;
		}
				
		case cmd_Copy:
		{
			outEnabled = LO_HaveSelection(*mContext);
			break;
		}

		case cmd_SelectAll:
		{
			outEnabled = (mContext->GetCurrentHistoryEntry() != NULL);
			break;
		}
		
		case cmd_Find:
		{
			outEnabled = (mContext->GetCurrentHistoryEntry() != NULL);
			break;
		}
		
		case cmd_FindAgain:
		{
			outEnabled = ((mContext->GetCurrentHistoryEntry() != NULL) && CFindWindow::CanFindAgain());
			break;
		}
		
		case cmd_PageSetup:
			if (CanPrint())
				outEnabled = TRUE;
			break;
		case cmd_Print:
		case cmd_PrintOne:
		{
			if (CanPrint())
				outEnabled = TRUE;
			if (mContext && (MWContext (*mContext)).is_grid_cell)
				LString::CopyPStr( GetPString( MENU_PRINT_FRAME ), outName);
			else
				LString::CopyPStr( GetPString( MENU_PRINT ), outName);
			break;
		}
		case cmd_MailDocument:	// Send Page/Frame
			if (mContext->IsGridCell())
			{	// frame view, set menu command to "Send Frame..."
				LStr255 sendFrameStr(cPaulsMiscStrListID, cSendFrameIndex);
				LString::CopyPStr(sendFrameStr, outName);
				outEnabled = !isHTMLHelp;
			}
			else if (!mContext->HasGridChildren() && !mContext->HasGridParent())
			{	// non-frame view
				LStr255 sendPageStr(cPaulsMiscStrListID, cSendPageIndex);
				LString::CopyPStr(sendPageStr, outName);
				outEnabled = !isHTMLHelp;
			}
			else
			{	// frame container view
				LStr255 sendFrameStr(cPaulsMiscStrListID, cSendFrameIndex);
				LString::CopyPStr(sendFrameStr, outName);
				outEnabled = false;
			}
			break;

		default:
			if(inCommand >= ENCODING_BASE && inCommand < ENCODING_CEILING)
			{
				outEnabled = true;
				outUsesMark = true;
				
				int16 csid = CPrefs::CmdNumToDocCsid( inCommand );
				outMark = (csid == mContext->GetDefaultCSID()) ? checkMark : ' ';
			} else 
				LTabGroup::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
	}
} // CHTMLView::FindCommandStatus


void CHTMLView::GetDefaultFileNameForSaveAs(URL_Struct* url, CStr31& defaultName)
{
	// Overridden by CMessageView to use the message subject instead.
	fe_FileNameFromContext(*mContext, url->address, defaultName);
}

Boolean	CHTMLView::ObeyCommand(CommandT inCommand, void* ioParam)
{
	Boolean cmdHandled = false;
//	CURLDispatcher*	theDispatcher = CURLDispatcher::GetURLDispatcher();
	CHTMLClickRecord* cr = mCurrentClickRecord; // for brevity.
		// cr will be non-null when handling a context menu command.
	mCurrentClickRecord = nil; // set it back.
	LClipboard* clip = LClipboard::GetClipboard();
	switch (inCommand)
	{			
		case cmd_NEW_WINDOW_WITH_FRAME:
			History_entry* history = mContext->GetCurrentHistoryEntry();
			if (!history)
				break;
			URL_Struct* url = SHIST_CreateURLStructFromHistoryEntry(*mContext, history);			
			if (url)
			{
				url->history_num    = 0;
				XP_MEMSET(&url->savedData, 0, sizeof(SHIST_SavedData));
			}
			
			DispatchURL(url, mContext, false, true);
			cmdHandled = true;
			break;
		case cmd_OPEN_LINK:
			if (cr)
			{
				if (cr->mClickKind == eImageIcon)
				{
					// Make sure that we follow the link, and not load the image
					cr->mClickKind = eImageAnchor;
				}
				ClickSelfLink(*(SMouseDownEvent*)ioParam, *cr, FALSE);
			}
			cmdHandled = true;
			break;
		case cmd_NEW_WINDOW:
			if (cr)
			{
				ClickSelfLink(*(SMouseDownEvent*)ioParam, *cr, TRUE);// case C
				cmdHandled = true;
			}
			break;
		case cmd_COPY_LINK_LOC:
			if (cr)
			{
				try													//Êcase D
				{
					if (cr->IsAnchor() && (cr->mClickURL.length() > 0))
						clip->SetData('TEXT', cr->mClickURL.data(), cr->mClickURL.length(), TRUE);
					else											// case E
					{
						cstring	urlString = mContext->GetCurrentURL();
						clip->SetData('TEXT', urlString.data(), urlString.length(), TRUE);
					}
				}
				catch(...) {}
				cmdHandled = true;
			}
			break;
		case cmd_VIEW_IMAGE:
			if (cr)
			{
				cstring urlString = GetURLFromImageElement(mContext, (LO_ImageStruct*) cr->mElement);
				if (urlString != "")
				{
					URL_Struct* url = NET_CreateURLStruct(urlString, NET_DONT_RELOAD); 
					DispatchURL(url, mContext);
				}
				cmdHandled = true;
			}
			break;
		case cmd_SAVE_IMAGE_AS:
			if (cr)
			{
				StandardFileReply reply;
				cstring urlString = GetURLFromImageElement(mContext, (LO_ImageStruct*) cr->mElement);
				if (urlString == "")
					break;
				CStr31 fileName = CFileMgr::FileNameFromURL( urlString );
				Boolean isMailAttachment = false;
				#ifdef MOZ_MAIL_NEWS
				isMailAttachment = XP_STRSTR( urlString, "?part=") || XP_STRSTR(urlString, "&part=");
				if( isMailAttachment ) 
				{
					fe_FileNameFromContext(*mContext, urlString, fileName);
				}
				#endif // MOZ_MAIL_NEWS
				
				StandardPutFile(GetPString(MCLICK_SAVE_IMG_AS), fileName, &reply);
				if (reply.sfGood)
				{
					URL_Struct* url = NET_CreateURLStruct(urlString, NET_DONT_RELOAD);
					XP_MEMSET(&url->savedData, 0, sizeof(SHIST_SavedData));
					CURLDispatcher::DispatchToStorage(url, reply.sfFile);
				}
				cmdHandled = true;
			}
			break;
		case cmd_COPY_IMAGE:
			if (cr)
			{
				PicHandle pict = ConvertImageElementToPICT((LO_ImageStruct*) cr->mElement);
				if (pict != NULL)
				{
					try
					{
						clip->SetData('PICT', (Handle) pict, TRUE);
					}
					catch(...) 
					{
					}
					KillPicture(pict);
				}
				cmdHandled = true;
			}
			break;
		case cmd_COPY_IMAGE_LOC:
			if (cr)
			{
				cstring urlString = GetURLFromImageElement(mContext, (LO_ImageStruct*) cr->mElement);
				if (urlString != "")
				{
					try
					{
						clip->SetData('TEXT', urlString, strlen(urlString), TRUE );
					}
					catch(...)
					{}
				}
				cmdHandled = true;
			}
			break;
		case cmd_LOAD_IMAGE:
			if (cr)
			{
				PostDeferredImage(cr->mImageURL);
				cmdHandled = true;
			}
			break;
			
		case cmd_LoadImages:
			{
				// Load images for the top-most browser context
				
				cmdHandled = true;
				
				if (GetContext())
				{
					CBrowserContext* theTopContext = GetContext()->GetTopContext();
					if (theTopContext)
					{
						LO_SetForceLoadImage(NULL, TRUE);
						theTopContext->Repaginate();
					}
				}
			}
			break;

		case cmd_ViewSource:
		{
			URL_Struct* url = NET_CreateURLStruct(mContext->GetCurrentURL(), NET_DONT_RELOAD);
			mContext->ImmediateLoadURL(url, FO_VIEW_SOURCE);
			NET_FreeURLStruct ( url );
			cmdHandled = true;
			break;
		}
		case cmd_AddToBookmarks:
		{
			// two cases: is this the "Add URL to bookmarks" from context menu or
			// using "Add Bookmarks" from the bookmark menu. 
			
			if ( cr && cr->IsAnchor() ) {
				// strip off the protocol then truncate the middle
				char trunkedAddress[HIST_MAX_URL_LEN + 1];
				string url = cr->GetClickURL();
				const char* strippedAddr = SHIST_StripProtocol( const_cast<char*>(url.c_str()) );
				INTL_MidTruncateString( 0, strippedAddr, trunkedAddress, HIST_MAX_URL_LEN );
				
				CBookmarksAttachment::AddToBookmarks( cr->GetClickURL(), trunkedAddress);
			}
			else {
				History_entry *entry = mContext->GetCurrentHistoryEntry();
				if ( entry )
					CBookmarksAttachment::AddToBookmarks(entry->address, entry->title);
			}
			break;
		}
		case cmd_SAVE_LINK_AS:
		case cmd_SaveAs:
		{
			URL_Struct* url = nil;
			if (cr && cr->IsAnchor() && inCommand == cmd_SAVE_LINK_AS )
				url = NET_CreateURLStruct(cr->mClickURL, NET_DONT_RELOAD);
			else
				url = NET_CreateURLStruct(mContext->GetCurrentURL(), NET_DONT_RELOAD);
			CStr31 fileName;
			Boolean isMailAttachment = false;
			#ifdef MOZ_MAIL_NEWS
			isMailAttachment = XP_STRSTR( url->address, "?part=") || XP_STRSTR(url->address, "&part=");
			if( isMailAttachment ) 
			{
				CHTMLView::GetDefaultFileNameForSaveAs(url, fileName);
			}
			else
			#endif // MOZ_MAIL_NEWS
			{
				if (inCommand == cmd_SAVE_LINK_AS)
				{
					fileName = CFileMgr::FileNameFromURL( url->address );
				}
				else
				{
					GetDefaultFileNameForSaveAs(url, fileName);
				}
			}
			
			StandardFileReply reply;
			short format;
			if (cr && cr->IsAnchor() && (!strncasecomp(url->address, "ftp://", 6) || isMailAttachment ) )
				(void) UStdDialogs::AskSaveAsSource(reply, fileName, format);
			else
				(void) UStdDialogs::AskSaveAsTextOrSource(reply, fileName, format);
			if (reply.sfGood)
			{
				short saveType = (format == 2) ? FO_SAVE_AS : FO_SAVE_AS_TEXT;
				CURLDispatcher::DispatchToStorage(url, reply.sfFile, saveType, isMailAttachment );
			}
			
			cmdHandled = true;
			break;
		}
		
		case cmd_Copy:
		{
			char*		realText;
			XP_Block	copyText = LO_GetSelectionText(*mContext);
			OSErr		err = ::ZeroScrap();
			
			XP_LOCK_BLOCK(realText, char*, copyText);
			err = ::PutScrap(strlen(realText), 'TEXT', (Ptr)realText);
			::TEFromScrap();
			XP_UNLOCK_BLOCK(copyText);
			XP_FREE_BLOCK(copyText);
			break;
		}
		
		case cmd_SelectAll:
		{
			(void) LO_SelectAll(*mContext);
			break;
		}

		case cmd_Find:
		{
			CFindWindow::DoFind(this);
			break;
		}
		
		case cmd_FindAgain:
		{
			DoFind();
			break;
		}
					
		case cmd_Print:
		case cmd_PrintOne:
		{
			DoPrintCommand(inCommand);
			cmdHandled = true;
			break;
		}

		case cmd_SecurityInfo:
		{
			MWContext* mwcontext = *GetContext();
			if (mwcontext)
			{
				URL_Struct* url =
					SHIST_CreateURLStructFromHistoryEntry(
						mwcontext,
						GetContext()->GetCurrentHistoryEntry()
						);
				
				// It's OK if we call SECNAV_SecurityAdvisor with a NULL URL
				// which is the case when the user has a blank page as their startup
				// BUG #66435
				//if (url)
					SECNAV_SecurityAdvisor(mwcontext,url);
			}
			break;
		}

		case cmd_DocumentInfo:
			URL_Struct* aboutURL = NET_CreateURLStruct( "about:document", NET_DONT_RELOAD );
			if ( aboutURL )
			{
				// about:document is displayed in the doc info window, so we don't need to call CURLDispatcher.
				// Just call SwitchLoadURL on mContext
				mContext->SwitchLoadURL(aboutURL, FO_CACHE_AND_PRESENT);
			}
			break;
		
		case cmd_FTPUpload:
		case msg_TabSelect:
			cmdHandled = true;
			break;
		
		case cmd_MailDocument:	// Send Page/Frame
			MSG_MailDocument(*mContext);
			break;

		default:
			cmdHandled = LTabGroup::ObeyCommand(inCommand, ioParam);
			break;
	}
	if (cr && cr->IsAnchor())
		LO_HighlightAnchor(*mContext, cr->mElement, FALSE);
	return cmdHandled;
} // CHTMLView::ObeyCommand

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
Boolean CHTMLView::SetDefaultCSID(Int16 default_csid)
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
{
	if (mContext && default_csid != mContext->GetDefaultCSID())
	{
		mContext->SetDefaultCSID(default_csid);
		mContext->SetWinCSID(INTL_DocToWinCharSetID(default_csid));
		mContext->Repaginate();
	}
	return true;
} // CHTMLView::SetDefaultCSID



// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
Boolean CHTMLView::CanPrint() const
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
{	
	// ¥ in order to be able to print we must
	//		not already be printing,
	//		not be busy loading,
	//		not be busy repaginating,
	//		have a page loaded,
	//		not be low on memory,
	//		and have a print record to print with
	return		mContext &&
				!XP_IsContextBusy(*mContext) &&
				!mContext->IsRepaginating() &&
				(mContext->GetCurrentHistoryEntry() != nil) && 
				!Memory_MemoryIsLow();
//				&& CPrefs::GetPrintRecord();
/*	We no longer checking for a print record; now we allow the user to make
	the menu selection and then provide feedback about its failure) */
} // CHTMLView::CanPrint() const

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
URL_Struct *CHTMLView::GetURLForPrinting(Boolean& outSuppressURLCaption, MWContext * /* context */)
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
{
	MWContext *context = *GetContext();
	History_entry* current = SHIST_GetCurrent(&context->hist);
	ThrowIfNil_(current);

	URL_Struct* url = SHIST_CreateWysiwygURLStruct(context, current);
	ThrowIfNil_(url);
	
	outSuppressURLCaption = false;
	
	return url;
} // CHTMLView::GetURLForPrinting()

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CHTMLView::DoPrintCommand(CommandT inCommand)
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
{
	THPrint hp;
	hp = CPrefs::GetPrintRecord();
	if ( !hp )
	{
		FE_Alert( *GetContext(), GetPString ( NO_PRINTER_RESID ) );
		return;
	}

	UPrintingMgr::ValidatePrintRecord( hp );

	Boolean printOne = ( inCommand == cmd_PrintOne );

	//
	// If this page is a single fullscreen plugin, give the
	// plugin a chance to handle the entire print process.  If
	// it returns false, it doesnÕt want to take over printing,
	// so we should print as normal.
	//
	Boolean doPrint = true;

	NPEmbeddedApp* app = ((MWContext*)*mContext)->pluginList;
	if (app != NULL && app->next == NULL && app->pagePluginType == NP_FullPage)
	{
		CPluginView* plugin = (CPluginView*) app->fe_data;
		if ( plugin )
			doPrint = ( plugin->PrintFullScreen( printOne, hp ) != TRUE );
	}

	//
	// There wasnÕt a fullscreen plugin, or there was a plugin 
	// but it wants us to take care of printing.  In this case
	// we should show the normal dialog if necessary and print.
	//
	if ( doPrint )
	{
		if ( printOne )
		{
			(**hp).prJob.iFstPage = 1;
			(**hp).prJob.iLstPage = max_Pages;
			(**hp).prJob.iCopies = 1;
			UHTMLPrinting::BeginPrint( hp, this );
		}
		else
		{
			if ( UPrintingMgr::AskPrintJob( hp ) )
				UHTMLPrinting::BeginPrint( hp, this );
		}
	}
} // CHTMLView::DoPrintCommand

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- FIND SUPPORT ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ



void CHTMLView::CreateFindWindow()
{
	LWindow::CreateWindow(6000, LCommander::GetTopCommander());
}

	// history code, I am not sure what does it do
static void SelectionToImage(LO_Element* startElement,
								int32 /*startPos*/, 
								LO_Element* /*endElement*/,
								int32 /*endPos*/, 
								SPoint32* topLeft,
								SPoint32* botRight)
{
	/*
	Calculate an image rectangle which is good enough to display some portion
	of a selection. Since selections can wrap around and do other wierd stuff,
	we'll make it big enough to cover the first "element" of the selection.
	*/
	
	topLeft->h = startElement->lo_any.x;
	topLeft->v = startElement->lo_any.y;
	
	botRight->h = startElement->lo_any.x + 64	/*endPos*/;
	botRight->v = startElement->lo_any.y + 20;
}

// Returns whether the search string was found - 1997-02-27 mjc
Boolean CHTMLView::DoFind()
{
	int32 			start_position;
	int32			end_position;
	LO_Element* 	start_ele_loc 	= NULL;
	LO_Element* 	end_ele_loc 	= NULL;
	CL_Layer* 		layer;
	MWContext* 		gridContext;
	MWContext* 		currentContext = *mContext;
	Boolean 		found = false;
	char* 			lookFor = CFindWindow::sLastSearch;
	Boolean			caseless = CFindWindow::sCaseless;
	Boolean			backward  = CFindWindow::sBackward;
	Boolean			doWrap = CFindWindow::sWrap;
	
	/*
		I didn't like doing that, that being "grabbing statics from a class"
		but it's a lot cleaner than before, and it works.  Mail gets this for
		free, and only need a few overrides to provide their own search dialog
		deeje 1997-01-22
	*/
	
	// work within the current selection
	LO_GetSelectionEndpoints(currentContext,
								&start_ele_loc,
								&end_ele_loc,
								&start_position,
								&end_position,
								&layer);

	do {		
		gridContext = currentContext;
		found = LO_FindGridText(currentContext,
								&gridContext,
								lookFor,
								&start_ele_loc,
								&start_position,
								&end_ele_loc,
								&end_position,
								!caseless,
								!backward);

		if (found)
			break;
		if (doWrap) {
			/* try again from the beginning.  these are the values LO_GetSelectionEndpoints
			   returns if there was no selection */
			start_ele_loc = NULL;
			end_ele_loc = NULL;
			start_position = 0;
			end_position = 0;
			doWrap = false;
		} else {
			SysBeep(1);
			return false;
		}
	} while (true);

	/* 6.6.97: set target even if we didn't switch cells.  do this in case the CHTMLView
	   isn't the target at all.  this is entirely possible after the first Find on
	   a new window. */
//	if (currentContext != gridContext)	// Text was found in a different cell
//	{
		SwitchTarget(gridContext->fe.newView);
//	}
	
	SPoint32	selTopLeft, selBotRight;
	int32		tlx, tly;
	
		// Scroll before selecting because otherwise hilighting gets screwed up
	SelectionToImage(start_ele_loc,
						start_position,
						end_ele_loc,
						end_position,
						&selTopLeft,
						&selBotRight);
	
	RevealInImage(gridContext->fe.newView, selTopLeft, selBotRight);
	LO_SelectText(gridContext,
					start_ele_loc,
					start_position,
					end_ele_loc,
					end_position,
					&tlx,
					&tly);
	return true;
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- MOUSING AND KEYING ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Converts image point to offset from the origin of the available screen rect.
//    That is, the screen origin + menubar height.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::ImageToAvailScreenPoint(const SPoint32 &inImagePoint, Point &outPoint) const
{
	ImageToLocalPoint(inImagePoint, outPoint);
	LocalToPortPoint(outPoint);
	PortToGlobalPoint(outPoint);
	outPoint.v -= ::GetMBarHeight();
}		

void CHTMLView::ClickSelf(const SMouseDownEvent& inMouseDown)
{
// With LAYERS, the original method gets the compositor to dispatch
// the event, which is then dealt with (in this method) on a per-layer
// basis.

	if ((mContext != NULL) && FocusDraw())
		{
		Int16 clickCount = GetClickCount();
		SPoint32 theImageClick;
		LocalToImagePoint(inMouseDown.whereLocal, theImageClick);
		if (mCompositor != NULL)
			{
			fe_EventStruct	fe_event;
			fe_event.portPoint = inMouseDown.wherePort;
			//fe_event.event =  (void *)&inMouseDown;
			fe_event.event.mouseDownEvent = inMouseDown; // 1997-02-25 mjc

			CL_Event		event;
			
			if (clickCount > 2) return; // ignore triple (or more) clicks
			event.type = CL_EVENT_MOUSE_BUTTON_DOWN;
			event.fe_event = (void *)&fe_event;
			event.fe_event_size = sizeof(fe_EventStruct); // 1997-02-25 mjc
			event.x = theImageClick.h;
			event.y = theImageClick.v;
			event.which = 1;
			event.data = clickCount;
			event.modifiers = CMochaHacks::MochaModifiers(inMouseDown.macEvent.modifiers);  // 1997-02-27 mjc
		
			// set a wait flag that gives ClickSelfLink and EventMouseUp a chance to execute,
			// and prevents the possibility of two mouse ups occurring for one mouse down.
			// ALERT:
			// This approach may have to be revised if we need to handle double clicks in JS.
			// This is because there seem to be some cases in which a double click might cause
			// EventMouseUp to be called when ClickSelfLink should be called instead. While this
			// has no adverse affect on the browser, a script may not expect that the mouse up
			// is not followed by a click.
			mWaitMouseUp = true;
			CL_DispatchEvent(*mCompositor, &event);	
			}
		else
			{
			mWaitMouseUp = true;
			ClickSelfLayer(inMouseDown, NULL, theImageClick);
			}
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Callback for click handling for new mocha event stuff	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// DoExecuteClickInLinkRecord
// Used as a Mocha callback structure
// holds all the information needed to call ReallyDoExecuteClickInLink
struct DoExecuteClickInLinkRecord
{
	SMouseDownEvent mWhere;
	CHTMLClickRecord mCr;
	Boolean mMakeNewWindow;
	Boolean mSaveToDisk;
	LO_AnchorData* mMouseOverMapArea; 	// for click event handling in image maps
	CHTMLView * mView;
	int32 mEventType;
	
	DoExecuteClickInLinkRecord(	CHTMLView * view, 
								const SMouseDownEvent& where,
								CHTMLClickRecord cr,
								Boolean makeNewWindow,
								Boolean saveToDisk,
								LO_AnchorData* mouseOverMapArea,
								int32 eventType)
								: mCr(cr)	// do we really want to use default copy contructor?
	{
		mView = view;
		mWhere = where;
		mMakeNewWindow = makeNewWindow;
		mSaveToDisk = saveToDisk;
		mMouseOverMapArea = mouseOverMapArea;
		mEventType = eventType;
	}
			
	void ExecuteClickInLink()
	{
		Assert_(mView != NULL);
		mView->PostProcessClickSelfLink(mWhere, mCr, mMakeNewWindow, mSaveToDisk, false);
	}
};

// Mocha callback for DoExecuteClickInLink
// Click preceded by mouse up - 1997-03-08 mjc
void MochaDoExecuteClickInLinkCallback(MWContext * pContext, LO_Element * lo_element, int32 lType, void * whatever, ETEventStatus status);
void MochaDoExecuteClickInLinkCallback(MWContext * pContext, 
										LO_Element * lo_element, 
                 						int32  /* lType */, 
                 						void * whatever, 
                 						ETEventStatus status)
{
	DoExecuteClickInLinkRecord * p = (DoExecuteClickInLinkRecord*)whatever;
	
	if (status == EVENT_OK)
	{
		if (p->mEventType == EVENT_MOUSEUP)
		{
			try {
				
				DoExecuteClickInLinkRecord* clickRecord =
					new DoExecuteClickInLinkRecord(p->mView,
												   p->mWhere,
												   p->mCr,
												   p->mMakeNewWindow,
												   p->mSaveToDisk,
												   p->mMouseOverMapArea,
												   EVENT_CLICK);
				
				JSEvent* event = XP_NEW_ZAP(JSEvent);
				if (event)
				{
					if (p->mMouseOverMapArea != NULL)
					{
						lo_element = XP_NEW_ZAP(LO_Element);	// Need to fake the element, ask chouck for details
						lo_element->type = LO_TEXT;
						lo_element->lo_text.anchor_href = p->mMouseOverMapArea;
						if (lo_element->lo_text.anchor_href->anchor)
							lo_element->lo_text.text = lo_element->lo_text.anchor_href->anchor; // to pass js freed element check
					}
					CL_Layer* layer = p->mCr.GetLayer();
					event->type = EVENT_CLICK;
					event->which = 1;
					event->x = p->mCr.GetImageWhere().h;
					event->y = p->mCr.GetImageWhere().v;
					event->docx = event->x + CL_GetLayerXOrigin(layer);
					event->docy = event->y + CL_GetLayerYOrigin(layer);
					
					Point screenPt = { event->docy, event->docx };
					SPoint32 imagePt;
					p->mView->LocalToImagePoint(screenPt, imagePt);
					p->mView->ImageToAvailScreenPoint(imagePt, screenPt);
					event->screenx = screenPt.h;
					event->screeny = screenPt.v;
					
					event->layer_id = LO_GetIdFromLayer(pContext, layer);
					event->modifiers = CMochaHacks::MochaModifiers(p->mWhere.macEvent.modifiers);
					ET_SendEvent(pContext,
								 lo_element,
								 event,
								 MochaDoExecuteClickInLinkCallback,
								 clickRecord);
				}
			} 
			catch (...) 
			{
			}
		}
		else if (p->mEventType == EVENT_CLICK && p->mCr.IsClickOnAnchor())
			p->ExecuteClickInLink();
	}
	// Free faked element after click
	if (p->mEventType == EVENT_CLICK && p->mMouseOverMapArea != NULL)
		XP_FREE(lo_element);
	delete p;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Mouse tracking.
// On click if it is an anchor, track it. If the mouse does not move, load the anchor.
// If it is not an anchor, continue the selection.

void CHTMLView::ClickSelfLayer(
	const SMouseDownEvent&		inMouseDown,
	CL_Layer*					inLayer,
	SPoint32 					inLayerWhere)
{
	SPoint32 theElementWhere = inLayerWhere;
	LO_Element* theElement = LO_XYToElement(*mContext, theElementWhere.h, theElementWhere.v, inLayer);
	
	CHTMLClickRecord cr(inMouseDown.whereLocal, inLayerWhere, mContext, theElement, inLayer);
	cr.Recalc();
	
	Boolean bClickHandled = false;

	// ¥ with shift key, just track selection
	if ((inMouseDown.macEvent.modifiers & shiftKey) == 0)
	{
		if ((theElement != NULL) && cr.PixelReallyInElement(theElementWhere, theElement))
		{
			// Move check for edge click here; otherwise, if you hold down mouse button over
			// edge, then context menu pops up. Whoops.
			if (cr.IsClickOnEdge())
				ClickTrackEdge(inMouseDown, cr);
			else if (cr.IsClickOnAnchor())
			{
				Int32 theClickStart = inMouseDown.macEvent.when;
				Point thePrevPoint = inMouseDown.whereLocal;

				LO_HighlightAnchor(*mContext, theElement, true);
		
				mCurrentClickRecord = &cr;
				CHTMLClickRecord::EClickState theMouseAction = CHTMLClickRecord::eUndefined;
				// Use a try block to ensure that we set it back to nil
				try
				{
					theMouseAction
						= cr.WaitForMouseAction(inMouseDown, this, GetDblTime(), ContextMenuPopupsEnabled());
				}
				catch(...) {}
				mCurrentClickRecord = nil;
				
				// Whether we need to unhighlight after handling the click.
				// In the mouse up early case, unhighlighting must be done before the
				// content changes, not afterwards, otherwise we would crash because the
				// element is no longer there.
				Boolean doUnhighlight = true;
				
				switch (theMouseAction)
				{
					case CHTMLClickRecord::eMouseDragging:
						ClickDragLink(inMouseDown, theElement);
						break;
					
					case CHTMLClickRecord::eMouseTimeout:
/*
					{
						Int16 thePopResult = this->DoPopup(inMouseDown, cr);
						if (thePopResult)
							this->HandlePopupResult(inMouseDown, cr, thePopResult);
						else
							LO_HighlightAnchor(*mContext, cr.mElement, false);
					}
*/
						break;
					
					case CHTMLClickRecord::eMouseUpEarly:
						ClickSelfLink(inMouseDown, cr, false);
						doUnhighlight = false;
						break;
					
					case CHTMLClickRecord::eHandledByAttachment:
						// Nothing else to do.
						break;
					default:
						SignalPStr_("\pUnhandled click case!!!");
						break;
				}
					
				if (doUnhighlight) LO_HighlightAnchor(*mContext, cr.mElement, false);
				bClickHandled = true;
			}
			else if ((theElement != NULL) && (theElement->type == LO_IMAGE))
			{
				// ¥ allow dragging of non-anchor images
				mCurrentClickRecord = &cr;
				CHTMLClickRecord::EClickState theMouseAction = CHTMLClickRecord::eUndefined;
				try
				{
					theMouseAction = cr.WaitForMouseAction(inMouseDown.whereLocal, inMouseDown.macEvent.when, GetDblTime());
				}
				catch(...) {}
				mCurrentClickRecord = nil;
				if (theMouseAction == CHTMLClickRecord::eMouseDragging)
				{
					ClickDragLink(inMouseDown, theElement);
					bClickHandled = true;
				}
			}
		}
	}
	
	// callback for form elements - mjc
	if (!bClickHandled)
	{
		if ((theElement != NULL) && 
			(theElement->type == LO_FORM_ELE) &&
			(inLayerWhere.h - theElement->lo_form.x - theElement->lo_form.x_offset <= theElement->lo_form.width) &&
			(inLayerWhere.h - theElement->lo_form.x - theElement->lo_form.x_offset >= 0) &&
			(inLayerWhere.v - theElement->lo_form.y - theElement->lo_form.y_offset <= theElement->lo_form.height) &&
			(inLayerWhere.v - theElement->lo_form.y - theElement->lo_form.y_offset >= 0))
		{
			switch (theElement->lo_form.element_data->type)
			{
				case FORM_TYPE_SUBMIT:
				case FORM_TYPE_RESET:
				case FORM_TYPE_BUTTON:
					{
						LPane*			thePane			= ((FormFEData *)theElement->lo_form.element_data->ele_minimal.FE_Data)->fPane;

						CFormButton*		theFormButton	= dynamic_cast<CFormButton*>(thePane);
						CGAFormPushButton*	theGAFormPushButton	= dynamic_cast<CGAFormPushButton*>(thePane);

						if (theFormButton)
						{						
							theFormButton->ClickSelfLayer(inMouseDown);
						}
						else if (theGAFormPushButton)
						{
							theGAFormPushButton->ClickSelfLayer(inMouseDown);
						}
						else
						{
							Assert_(false);
						}

						bClickHandled = true;
					}
					break;
				case FORM_TYPE_RADIO:
					{
						LPane*			thePane			= ((FormFEData *)theElement->lo_form.element_data->ele_minimal.FE_Data)->fPane;

						CFormRadio*		theFormRadio	= dynamic_cast<CFormRadio*>(thePane);
						CGAFormRadio*	theGAFormRadio	= dynamic_cast<CGAFormRadio*>(thePane);

						if (theFormRadio)
						{						
							theFormRadio->ClickSelfLayer(inMouseDown);
						}
						else if (theGAFormRadio)
						{
							theGAFormRadio->ClickSelfLayer(inMouseDown);
						}
						else
						{
							Assert_(false);
						}

						bClickHandled = true;
					}
					break;
				case FORM_TYPE_CHECKBOX:
					{
						LPane*				thePane				= ((FormFEData *)theElement->lo_form.element_data->ele_minimal.FE_Data)->fPane;

						CFormCheckbox*		theFormCheckbox		= dynamic_cast<CFormCheckbox*>(thePane);
						CGAFormCheckbox*	theGAFormCheckbox	= dynamic_cast<CGAFormCheckbox*>(thePane);

						if (theFormCheckbox)
						{						
							theFormCheckbox->ClickSelfLayer(inMouseDown);
						}
						else if (theGAFormCheckbox)
						{
							theGAFormCheckbox->ClickSelfLayer(inMouseDown);
						}
						else
						{
							Assert_(false);
						}

						bClickHandled = true;
					}
					break;
			}
		}
	}
	
		// ¥¥¥Êthere must be a better way...
		//
		// this is just about the same code as above for handling a click
		// but it's repeated here because the code above handles clicks on graphical elements?
		//
		// also, I see two version of WaitForMouseAction being called, one global and one inside cr
		// which is appropriate?
		//
		// 96-12-18 deeje
	
	if (!bClickHandled && ContextMenuPopupsEnabled())
	{
		mCurrentClickRecord = &cr;
		CHTMLClickRecord::EClickState mouseAction = CHTMLClickRecord::eUndefined;
		try
		{
			mouseAction = CHTMLClickRecord::WaitForMouseAction(
				inMouseDown, this, GetDblTime());
		}
		catch (...) {}
		mCurrentClickRecord = nil;
		if ( mouseAction == CHTMLClickRecord::eHandledByAttachment )
			bClickHandled = TRUE;
/*
		if ( mouseAction == CHTMLClickRecord::eMouseTimeout ) // No popup when we have grids
		{
			Int16	thePopResult = this->DoPopup(inMouseDown, cr);
			if (thePopResult)
				this->HandlePopupResult(inMouseDown, cr, thePopResult);
			
			bClickHandled = TRUE;
		}
*/
	}

	if (!bClickHandled)
		{
			Boolean didTrackSelection = ClickTrackSelection(inMouseDown, cr);
			// if no selection, and we're not over an element,
			// send a mouse up and click to the document
			if (!didTrackSelection && (cr.mClickKind == eNone))
			{
				EventRecord macEvent;
		
				GetOSEvent(mUpMask, &macEvent); // grab the mouse up event so LEventDispatcher never sees it

				mWaitMouseUp = false;
				try {
					DoExecuteClickInLinkRecord* clickRecord =
						new DoExecuteClickInLinkRecord(this,
													   inMouseDown,
													   cr,
													   false,
													   false,
													   NULL,
													   EVENT_MOUSEUP);
					JSEvent* event = XP_NEW_ZAP(JSEvent);
					if (event)
					{
						// 97-06-10 pkc -- Turn off grab mouse events here because we don't call
						// compositor for mouse ups.
						MWContext* mwContext = *mContext;
						if (mwContext->js_dragging==TRUE) {
						    CL_GrabMouseEvents(mwContext->compositor, NULL);
							mwContext->js_dragging = 0;
						}

						event->type = EVENT_MOUSEUP;
						event->which = 1;
						event->x = cr.mImageWhere.h;
						event->y = cr.mImageWhere.v;
						event->docx = event->x + CL_GetLayerXOrigin(cr.mLayer);
						event->docy = event->y + CL_GetLayerYOrigin(cr.mLayer);
						
						Point screenPt = { event->docy, event->docx };
						SPoint32 imagePt;
						LocalToImagePoint(screenPt, imagePt);
						ImageToAvailScreenPoint(imagePt, screenPt);
						event->screenx = screenPt.h;
						event->screeny = screenPt.v;
						
						event->layer_id = LO_GetIdFromLayer(*mContext, cr.mLayer);
						event->modifiers = CMochaHacks::MochaModifiers(inMouseDown.macEvent.modifiers);
						ET_SendEvent(*mContext,
									 NULL,
									 event,
									 MochaDoExecuteClickInLinkCallback,
									 clickRecord);
					}
				} catch (...) {
				}
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::ClickSelfLink(
	const SMouseDownEvent&	inMouseDown,
	CHTMLClickRecord&		inClickRecord,
	Boolean					inMakeNewWindow)
{
	// Execute this only if there was a previous mouse down which did not have a corresponding
	// mouse up.
	if (mWaitMouseUp)
	{
		EventRecord macEvent;
		
		GetOSEvent(mUpMask, &macEvent); // grab the mouse up event so LEventDispatcher never sees it
		
		mWaitMouseUp = false; // got a mouse up so clear the wait flag.
		
		Boolean bSaveToDisk = ((inMouseDown.macEvent.modifiers & optionKey) == optionKey)
							  && !inMakeNewWindow;

		// do we also have to use a StTempFormBlur in PostProcessClickSelfLink?
		StTempFormBlur	tempBlur;	// mforms.h for an explanation
		
		// check for event handler in containing contexts or on anchor for the events
		// we are about to send.
		LO_Element* element = inClickRecord.GetLayoutElement();
		if (HasEventHandler(*mContext, (EVENT_MOUSEUP | EVENT_CLICK)) ||
			// map area anchor with an event handler
			(CMochaHacks::GetMouseOverMapArea() && CMochaHacks::GetMouseOverMapArea()->event_handler_present) ||
			(element && 
			// ALERT: event_handler_present doesn't seem to be initialized properly so test against TRUE.
			((element->type == LO_TEXT && element->lo_text.anchor_href && (element->lo_text.anchor_href->event_handler_present == TRUE)) ||
			 (element->type == LO_IMAGE && element->lo_image.anchor_href && (element->lo_image.anchor_href->event_handler_present == TRUE)) ||
			 // a plain image without an anchor could have event handlers, but there is no bit to tell us that.
			 ((element->type == LO_IMAGE) && 
			 	(element->lo_image.anchor_href == NULL) &&
			 	(element->lo_image.image_attr->usemap_name == NULL) && // not a client-side image map
			 	!(element->lo_image.image_attr->attrmask & LO_ATTR_ISMAP))))) // not a server-side image map			 	
		{
			try {
				DoExecuteClickInLinkRecord* clickRecord =
					new DoExecuteClickInLinkRecord(this,
												   inMouseDown,
												   inClickRecord,
												   inMakeNewWindow,
												   bSaveToDisk,
												   CMochaHacks::GetMouseOverMapArea(),
												   EVENT_MOUSEUP);
				// ET_SendEvent now takes a JSEvent struct instead of an int type
				JSEvent* event = XP_NEW_ZAP(JSEvent);
				if (event)
				{
					// 97-06-10 pkc -- Turn off grab mouse events here because we don't call
					// compositor for mouse ups.
					MWContext* mwContext = *mContext;
					if (mwContext->js_dragging==TRUE) {
					    CL_GrabMouseEvents(mwContext->compositor, NULL);
						mwContext->js_dragging = 0;
					}

					event->type = EVENT_MOUSEUP;
					event->which = 1;
					// send layer relative position info to mocha - mjc
					event->x = inClickRecord.mImageWhere.h;
					event->y = inClickRecord.mImageWhere.v;
					event->docx = event->x + CL_GetLayerXOrigin(inClickRecord.mLayer);
					event->docy = event->y + CL_GetLayerYOrigin(inClickRecord.mLayer);
										
					Point screenPt;
					ImageToAvailScreenPoint(inClickRecord.mImageWhere, screenPt);
					event->screenx = screenPt.h;
					event->screeny = screenPt.v;
					
					event->layer_id = LO_GetIdFromLayer(*mContext, inClickRecord.mLayer); // 1997-03-02 mjc
					event->modifiers = CMochaHacks::MochaModifiers(inMouseDown.macEvent.modifiers); // 1997-02-27 mjc
					ET_SendEvent(*mContext,
								 inClickRecord.mElement,
								 event,
								 MochaDoExecuteClickInLinkCallback,
								 clickRecord);
				}
			} catch (...) {
			}
		}
		// We delay DispatchToView because we're completely handling the mouse 
		// down in the front end, so the compositor needs to unwind before we switch content.
		else PostProcessClickSelfLink(inMouseDown, inClickRecord, inMakeNewWindow, bSaveToDisk, true);
	}
}

// PostProcessClickSelfLink
// Mocha callback routine, post processes DoExecuteClickInLink
// Because of the new threaded mocha, we have to wait for a call back
// after mocha calls, this code used to come after the call to LM_SendOnClick in ClickSelfLink.
// Now, this code gets executed after the call to ET_SendEvent calls us back through
// MochaDoExecuteClickInLinkCallback.
//
// inDelay - set to true if there is a possibility that the content will change before the compositor
// has finished unwinding from the mouse down event dispatch. If there were intervening
// javascript or compositor calls, for example, there is no need to delay.
void CHTMLView::PostProcessClickSelfLink(
	const SMouseDownEvent&	/* inMouseDown */,
	CHTMLClickRecord&		inClickRecord,
	Boolean					inMakeNewWindow,
	Boolean					inSaveToDisk,
	Boolean					inDelay)
{
	// Sniff the URL to detect mail attachment.  Set inSaveToDisk accordingly before
	// dispatching.  Note that in Constellation, mail messages may be viewed in a browser
	// view!

/*
	Boolean isMailAttachment = false;
	const char* url = inClickRecord.GetClickURL();
	if (!strncasecomp (url, "mailbox:", 8))
	{
	  if (XP_STRSTR(url, "?part=") || XP_STRSTR(url, "&part="))
		isMailAttachment = true; // Yep, mail attachment.
	}
*/

	CBrowserContext* theSponsorContext = (inMakeNewWindow) ? NULL : mContext;

	EClickKind oldClickKind = inClickRecord.mClickKind;

	inClickRecord.Recalc();
	
	switch (inClickRecord.mClickKind)
	{
		case eTextAnchor:
		case eImageAnchor:
		case eImageIsmap:
		case eImageAltText:
			{
			// unhighlight the anchor while it is still around (before content is switched)
			LO_HighlightAnchor(*mContext, inClickRecord.mElement, false);
			
			FO_Present_Types theType = (inSaveToDisk) ? FO_SAVE_AS : FO_CACHE_AND_PRESENT;
			URL_Struct* theURL = NET_CreateURLStruct(inClickRecord.mClickURL, NET_DONT_RELOAD);
			ThrowIfNULL_(theURL);

			cstring theReferer = mContext->GetCurrentURL();
			if (theReferer.length() > 0)
				theURL->referer = XP_STRDUP(theReferer);

			if (inMakeNewWindow)
				theURL->window_target = XP_STRDUP("");
			else
				{
				if (XP_STRCMP(inClickRecord.mWindowTarget, "") != 0)
					theURL->window_target = XP_STRDUP(inClickRecord.mWindowTarget);
				else
					theURL->window_target = NULL;
				}
			
			if (FO_SAVE_AS == theType)
			{
				// See also the code for popup stuff.  Combine these later.  FIX ME.
				CStr31 fileName = CFileMgr::FileNameFromURL( theURL->address );
				//fe_FileNameFromContext(*mContext, theURL->address, fileName);
				StandardFileReply reply;
/*
				if (isMailAttachment)
				{
					// ad decoder will prompt for the name.
					reply.sfGood = true;
					::FSMakeFSSpec(0, 0, fileName, &reply.sfFile);
				}
				else
*/
					::StandardPutFile(GetPString(SAVE_AS_RESID), fileName, &reply);
				if (reply.sfGood)
				{
					CURLDispatcher::DispatchToStorage(theURL, reply.sfFile, FO_SAVE_AS, false);
				}
			}
			else if (FO_CACHE_AND_PRESENT == theType)
			{
					DispatchURL(theURL, theSponsorContext, inDelay, inMakeNewWindow);
			}
			break;
		}

		case eImageForm:	// Image that is an ISMAP form
			{
/*			LO_FormSubmitData *theSubmit = NULL;
			try
				{
				theSubmit = LO_SubmitImageForm(*mContext, &inClickRecord.mElement->lo_image, inClickRecord.mImageWhere.h, inClickRecord.mImageWhere.v);
				ThrowIfNULL_(theSubmit);
				
				URL_Struct* theURL = NET_CreateURLStruct((char*)theSubmit->action, NET_DONT_RELOAD); 
				ThrowIfNULL_(theURL);
				
				cstring theCurrentURL = mContext->GetCurrentURL();
				if (theCurrentURL.length() > 0)
					theURL->referer = XP_STRDUP(theCurrentURL);

				if (NET_AddLOSubmitDataToURLStruct(theSubmit, theURL))
					theDispatcher->DispatchToView(theSponsorContext, theURL, FO_CACHE_AND_PRESENT, inMakeNewWindow, 1010, inDelay);

				LO_FreeSubmitData(theSubmit);
				}
			catch (...)
				{
				LO_FreeSubmitData(theSubmit);
				throw;			
				}*/
						// ET_SendEvent now takes a JSEvent struct instead of an int type
			ImageFormSubmitData* data = new ImageFormSubmitData;
			LO_ImageStruct* image = &inClickRecord.mElement->lo_image;
			data->lo_image = image;
			// set the coordinates relative to the image (not the document), taking into account the border.
			data->x = inClickRecord.mImageWhere.h - (image->x + image->x_offset + image->border_width);
			data->y = inClickRecord.mImageWhere.v - (image->y + image->y_offset + image->border_width);
			JSEvent* event = XP_NEW_ZAP(JSEvent);
			if (event)
				{
				event->type = EVENT_SUBMIT;
				// The code above will get executed in MochaFormSubmitCallback
				ET_SendEvent(*mContext,
							 inClickRecord.mElement,
							 event,
							 MochaImageFormSubmitCallback,
							 data);
				}
			}
			break;

		case eImageIcon:
			{
			if (!inSaveToDisk)
				{
				HandleImageIconClick(inClickRecord);
				}
			else
				{
				URL_Struct* theURL = NET_CreateURLStruct(inClickRecord.mClickURL, NET_DONT_RELOAD);
				if (theURL == NULL)
					break;

				cstring theReferer = theURL->address;
				if (theReferer.length() > 0)
					theURL->referer = XP_STRDUP(theReferer);

				if (inMakeNewWindow)
					{
					theURL->window_target = XP_STRDUP("");
					if (XP_STRCMP(inClickRecord.mWindowTarget, "") != 0)
						theURL->window_target = XP_STRDUP(inClickRecord.mWindowTarget);
					else
						theURL->window_target = NULL;
					}
	
				DispatchURL(theURL, theSponsorContext, false, inMakeNewWindow);
				}
			}
			break;

		case eNone:
			{
			// 97-06-10 pkc -- Hack to dispatch URL's after mocha is done loading image.
			// If oldClickKind is eImageAnchor, then we're in the case where mocha is loading
			// an image over a current image.
			if (oldClickKind == eImageAnchor && (((char*)inClickRecord.mClickURL) != NULL))
				{
				URL_Struct* theURL = NET_CreateURLStruct(inClickRecord.mClickURL, NET_DONT_RELOAD);
				ThrowIfNULL_(theURL);

				cstring theReferer = mContext->GetCurrentURL();
				if (theReferer.length() > 0)
					theURL->referer = XP_STRDUP(theReferer);

				if (inMakeNewWindow)
					theURL->window_target = XP_STRDUP("");
				else
					{
					if (XP_STRCMP(inClickRecord.mWindowTarget, "") != 0)
						theURL->window_target = XP_STRDUP(inClickRecord.mWindowTarget);
					else
						theURL->window_target = NULL;
					}
				// Call DispatchToView with inDelay = true and inWaitingForMochaImageLoad = true
//				theDispatcher->DispatchToView(theSponsorContext, theURL, FO_CACHE_AND_PRESENT, inMakeNewWindow, 1010, true, true, true);
				CURLDispatcher::DispatchURL(theURL, theSponsorContext, true, inMakeNewWindow, BrowserWindow_ResID, true, FO_CACHE_AND_PRESENT, true);
				}
			}
			break;

	
		default:
		case eWaiting:
			Assert_(false);
			break;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CHTMLView::Click(SMouseDownEvent &inMouseDown)
//	Special processing for drag-n-drop stuff, not to select the window until mouse up.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
{
	LPane *clickedPane = FindSubPaneHitBy(inMouseDown.wherePort.h,
										  inMouseDown.wherePort.v);
	if ( clickedPane != nil )
		clickedPane->Click(inMouseDown);
	else
	{
		if ( !inMouseDown.delaySelect ) LCommander::SwitchTarget(this);
		StValueChanger<Boolean> change(inMouseDown.delaySelect, false);
			// For drag-n-drop functionality
		LPane::Click(inMouseDown);	//   will process click on this View
	}
} // CHTMLView::Click

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::HandleImageIconClick(CHTMLClickRecord& inClickRecord)
{
	char* url = inClickRecord.mImageURL;
	// check to see if the click was on the attachment or SMime icon
	if (!XP_STRNCMP(url, "internal-smime", 14) || !XP_STRNCMP(url, "internal-attachment-icon", 24) )
	{
		URL_Struct* theURL = NET_CreateURLStruct(inClickRecord.mClickURL, NET_DONT_RELOAD);
		mContext->SwitchLoadURL(theURL, FO_CACHE_AND_PRESENT);
	}
	else
		PostDeferredImage(url);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

	//

void CHTMLView::ClickDragLink(
	const SMouseDownEvent& 	inMouseDown,
	LO_Element* 			inElement)
{	
	FocusDraw();

	Rect theGlobalRect;
	Boolean bVisible = CalcElementPosition(inElement, theGlobalRect);
	::LocalToGlobal(&topLeft(theGlobalRect));
	::LocalToGlobal(&botRight(theGlobalRect));

	// account for image border
	Int32 borderWidth = 0;
	
	if (inElement->type == LO_IMAGE)
		borderWidth = inElement->lo_image.border_width;

	::OffsetRect(&theGlobalRect, borderWidth, borderWidth);
	
	StValueChanger<LO_Element*>	theValueChanger(mDragElement, inElement);
	
	CDragURLTask theDragTask(inMouseDown.macEvent, theGlobalRect, *this);
	
	OSErr theErr = ::SetDragSendProc(theDragTask.GetDragReference(), mSendDataUPP, (LDragAndDrop*)this);
	ThrowIfOSErr_(theErr);
	
	theDragTask.DoDrag();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::ClickTrackEdge(
	const SMouseDownEvent&	inMouseDown,
	CHTMLClickRecord&		inClickRecord)
{
// ¥¥¥ FIX ME!!! we need to stop the blinkers

	Rect limitRect, slopRect;
	Int16 axis;
	
	// Figure out the outline of the region. This should come from layout somehow
	Point topRgn = inMouseDown.whereLocal;		
	Point bottomRgn = inMouseDown.whereLocal;
	if ( inClickRecord.mIsVerticalEdge )	// Edge is vertical, we can drag horizontally.
		{
		topRgn.v = inClickRecord.mElement->lo_edge.y;
		bottomRgn.v = topRgn.v + inClickRecord.mElement->lo_edge.height;
		topRgn.h = inClickRecord.mElement->lo_edge.x;
		bottomRgn.h = topRgn.h + inClickRecord.mElement->lo_edge.width;
		limitRect.left = inClickRecord.mEdgeLowerBound;
		limitRect.right = inClickRecord.mEdgeUpperBound;
		limitRect.top = topRgn.v;
		limitRect.bottom = bottomRgn.v;
		slopRect = limitRect;
		axis = hAxisOnly;
		}
	else
		{
		topRgn.h = inClickRecord.mElement->lo_edge.x;
		bottomRgn.h = topRgn.h + inClickRecord.mElement->lo_edge.width;
		topRgn.v = inClickRecord.mElement->lo_edge.y;
		bottomRgn.v = topRgn.v + inClickRecord.mElement->lo_edge.height;
		limitRect.top = inClickRecord.mEdgeLowerBound;
		limitRect.bottom = inClickRecord.mEdgeUpperBound;
		limitRect.left = topRgn.h;
		limitRect.right = bottomRgn.h;
		slopRect = limitRect;
		axis = vAxisOnly;
		}
	
	Rect	tmp;
	tmp.top = topRgn.v;
	tmp.left = topRgn.h;
	tmp.right = bottomRgn.h;
	tmp.bottom = bottomRgn.v;
	
	StRegion		rgn(tmp);
	FocusDraw();

	long movedBy = ::DragGrayRgn(rgn, inMouseDown.whereLocal, &limitRect, &slopRect, axis, NULL);
	if ( HiWord( movedBy ) != -32768 )	//	We have a valid movement
		{
		// We set a null clip so we don't flicker and flash
		StClipRgnState theSavedClip(NULL);

		LocalToPortPoint(topLeft(tmp));
		LocalToPortPoint(botRight(tmp));
		::InsetRect(&tmp, -2, -2);
		InvalPortRect(&tmp);

		Point newPoint;
		Rect newRect = tmp;
		if ( axis == vAxisOnly )
			{
			newPoint.v = inMouseDown.whereLocal.v + HiWord(movedBy);
			::OffsetRect(&newRect, 0, HiWord(movedBy));
			}
		else
			{
			newPoint.h = inMouseDown.whereLocal.h + LoWord( movedBy );
			::OffsetRect(&newRect, LoWord(movedBy), 0);
			}

		if (inClickRecord.mIsVerticalEdge)
			::InsetRect(&newRect, -RectWidth(limitRect), -3);
		else
			::InsetRect(&newRect, -3, -RectHeight(limitRect));

		::UnionRect(&tmp, &newRect, &tmp); 

			{
				StClipRgnState theClipSaver(NULL);
				// 97-06-11 pkc -- Set mDontAddGridEdgeToList to true so that DisplayEdge
				// doesn't add this grid edge into list and DrawGridEdge doesn't draw
				// because LO_MoveGridEdge will call DisplayEdge
				mDontAddGridEdgeToList = true;
				LO_MoveGridEdge(*mContext, (LO_EdgeStruct*)inClickRecord.mElement, newPoint.h, newPoint.v );
				mDontAddGridEdgeToList = false;
			}
		
		InvalPortRect( &tmp );
		}

// FIX ME!!! we need to start the blinkers
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
inline void CHTMLView::LocalToLayerCoordinates(
	const XP_Rect& inBoundingBox,
	Point inWhereLocal,
	SPoint32& outWhereLayer) const
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
{
	LocalToImagePoint(inWhereLocal, outWhereLayer);
	// Convert from image to layer coordinates
	outWhereLayer.h -= inBoundingBox.left;
	outWhereLayer.v -= inBoundingBox.top;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
Boolean CHTMLView::ClickTrackSelection(
	const SMouseDownEvent&	inMouseDown,
	CHTMLClickRecord&		inClickRecord)
//	¥ Returns true if a selection was started and extended.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
{
// FIX ME!!! we need to stop the blinkers
		
	::SetCursor(*GetCursor(iBeamCursor));
	XP_Rect theBoundingBox;
	CL_GetLayerBboxAbsolute(inClickRecord.mLayer, &theBoundingBox);

	// Convert from image to layer coordinates
	SPoint32 theImagePointStart;
	LocalToLayerCoordinates(theBoundingBox, inMouseDown.whereLocal, theImagePointStart);

	SPoint32 theImagePointEnd = theImagePointStart;

	if ((inMouseDown.macEvent.modifiers & shiftKey) && !inClickRecord.IsClickOnAnchor())
		LO_ExtendSelection(*mContext, theImagePointStart.h, theImagePointStart.v);
	else
		LO_StartSelection(*mContext, theImagePointStart.h, theImagePointStart.v, inClickRecord.mLayer);

	// track mouse till we are done
	Boolean didTrackSelection = false;
	while (::StillDown())
	{
		Point qdWhere;
		FocusDraw();	// so that we get coordinates right
		::GetMouse(&qdWhere);
		if (AutoScrollImage(qdWhere))
		{	
			Rect theFrame;
			CalcLocalFrameRect(theFrame);
			if (qdWhere.v < theFrame.top)
				qdWhere.v = theFrame.top;
			else if (qdWhere.v > theFrame.bottom)
				qdWhere.v = theFrame.bottom;
		}
	
		SPoint32 theTrackImagePoint;
		LocalToLayerCoordinates(theBoundingBox, qdWhere, theTrackImagePoint);

		if (theTrackImagePoint.v != theImagePointEnd.v
		|| theTrackImagePoint.h != theImagePointEnd.h)
		{
			didTrackSelection = true;
			LO_ExtendSelection(*mContext, theTrackImagePoint.h, theTrackImagePoint.v);
		}
		theImagePointEnd = theTrackImagePoint;

		// ¥Êidling
		::SystemTask();
		EventRecord dummy;
		::WaitNextEvent(0, &dummy, 5, NULL);
	}
			
	LO_EndSelection(*mContext);
	return didTrackSelection;
	
// FIX ME!!! we need to restart the blinkers
} // CHTMLView::ClickTrackSelection

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CHTMLView::EventMouseUp(const EventRecord& inMouseUp)
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
{
	// FocusDraw() && SwitchTarget(this) appears to be a copy/paste error from ClickSelf.
	// We will dispatch a mouse up only if a mouse down has been dispatched. Otherwise there
	// would be a possibility of this getting called if a click selects the window containing
	// this view but does not perform a mouse down.
	if (mContext != NULL && mWaitMouseUp) /* &&  FocusDraw() && SwitchTarget(this) */
		{
		SPoint32		firstP;
		Point			localPt = inMouseUp.where;
	
		mWaitMouseUp = false;
		GlobalToPortPoint( localPt);	// 1997-02-27 mjc
		PortToLocalPoint( localPt );
		LocalToImagePoint(	localPt, firstP );
		
		if (mCompositor != NULL)
			{
			fe_EventStruct	fe_event;
			fe_event.portPoint = inMouseUp.where;
			//fe_event.event =  (void *)&inMouseUp;
			fe_event.event.macEvent = inMouseUp; 	// 1997-02-27 mjc
		
			CL_Event		event;
			event.type = CL_EVENT_MOUSE_BUTTON_UP;
			event.fe_event = (void *)&fe_event;
			event.fe_event_size = sizeof(fe_EventStruct); // 1997-02-27 mjc
			event.x = firstP.h;
			event.y = firstP.v;
			event.which = 1;
			event.modifiers = CMochaHacks::MochaModifiers(inMouseUp.modifiers);
		
			CL_DispatchEvent(*mCompositor, &event);	
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CHTMLView::HandleKeyPress( const EventRecord& inKeyEvent )
{
	
	Char16		theChar = inKeyEvent.message & charCodeMask;
	short 		modifiers = inKeyEvent.modifiers & (cmdKey | shiftKey | optionKey | controlKey);
	SPoint32		firstP;
	Point			localPt = inKeyEvent.where;

	GlobalToPortPoint(localPt);
	PortToLocalPoint( localPt );
	LocalToImagePoint(	localPt, firstP );
	
#ifdef LAYERS
	if (mCompositor)
	{
		// If no one has key event focus, set event focus to the main document.
		if (CL_IsKeyEventGrabber(*mCompositor, NULL) && CL_GetCompositorRoot(*mCompositor))
		{
			CL_GrabKeyEvents(
							*mCompositor, 
							CL_GetLayerChildByName(
													CL_GetCompositorRoot(*mCompositor),
													LO_BODY_LAYER_NAME));
		}
		
		// If there's a compositor and someone has keyboard focus, dispatch the event.
		if (!CL_IsKeyEventGrabber(*mCompositor, NULL) && (sLastFormKeyPressDispatchTime != inKeyEvent.when)) 
		{
			CL_Event		event;
			fe_EventStruct	fe_event;

			fe_event.event.macEvent = inKeyEvent; // 1997-02-25 mjc

			event.data = 0;
			switch (inKeyEvent.what)
			{
				case keyDown:
					event.type = CL_EVENT_KEY_DOWN;
					break;
				case autoKey:
					event.type = CL_EVENT_KEY_DOWN;
					event.data = 1; // repeating
					break;
				case keyUp:
					event.type = CL_EVENT_KEY_UP;
					break;
			}

			event.fe_event = (void *)&fe_event;
			event.fe_event_size = sizeof(fe_EventStruct);
			event.x = firstP.h;
			event.y = firstP.v;
			event.which = theChar;
			event.modifiers = CMochaHacks::MochaModifiers(inKeyEvent.modifiers);  // 1997-02-27 mjc

			CL_DispatchEvent(*mCompositor, &event);
			return true;
		}
		else return HandleKeyPressLayer(inKeyEvent, NULL, firstP);
	} else
#endif // LAYERS
	return HandleKeyPressLayer(inKeyEvent, NULL, firstP);
}

Boolean CHTMLView::HandleKeyPressLayer(const EventRecord&	inKeyEvent,
									CL_Layer			*inLayer,
									SPoint32			inLayerWhere )
{
	Char16		theChar = inKeyEvent.message & charCodeMask;
	short 		modifiers = inKeyEvent.modifiers & (cmdKey | shiftKey | optionKey | controlKey);
	Boolean 	handled = false;
	
#ifdef LAYERS
	if ((inLayer != NULL) && (sLastFormKeyPressDispatchTime != inKeyEvent.when))
	{
		SPoint32 	theElementWhere = inLayerWhere;
		LO_Element* theElement = LO_XYToElement(*mContext, theElementWhere.h, theElementWhere.v, inLayer);

		if ((theElement != NULL) && 
			(theElement->type == LO_FORM_ELE))
		{
			switch (theElement->lo_form.element_data->type)
			{
				case FORM_TYPE_TEXTAREA:
					CFormBigText *bigText = (CFormBigText *)((FormFEData *)theElement->lo_form.element_data->ele_minimal.FE_Data)->fPane->FindPaneByID(formBigTextID);
					sLastFormKeyPressDispatchTime = inKeyEvent.when;
					handled =  bigText->HandleKeyPress(inKeyEvent);
					break;	
				case FORM_TYPE_TEXT:
				case FORM_TYPE_PASSWORD:
					CFormLittleText *text = (CFormLittleText *)((FormFEData *)theElement->lo_form.element_data->ele_minimal.FE_Data)->fPane;
					sLastFormKeyPressDispatchTime = inKeyEvent.when;
					handled = text->HandleKeyPress(inKeyEvent);
					break;
			}
		}
	}
#endif
	// forms didn't handle the event
	if (!handled)
	{
		// we may get keyUp events (javascript needs them) but just ignore them - 1997-02-27 mjc
		if (keyUp == inKeyEvent.what) return TRUE;
		
		if ( modifiers == 0 )
			switch ( theChar & charCodeMask )
			{
				case char_UpArrow:
					if ( mScroller && mScroller->HasVerticalScrollbar())
						mScroller->VertScroll( kControlUpButtonPart );
				return TRUE;
				case char_DownArrow:
					if ( mScroller && mScroller->HasVerticalScrollbar() )
					 	mScroller->VertScroll( kControlDownButtonPart );
				return TRUE;
				
				// Seems only fair to allow scrolling left and right like the other platforms
				case char_LeftArrow:
					if ( mScroller && mScroller->HasHorizontalScrollbar())
						mScroller->HorizScroll( kControlUpButtonPart );
				return TRUE;
				case char_RightArrow:
					if ( mScroller && mScroller->HasHorizontalScrollbar() )
					 	mScroller->HorizScroll( kControlDownButtonPart );
				return TRUE;

				case char_PageUp:
				case char_Backspace:
					if ( mScroller && mScroller->HasVerticalScrollbar())
						mScroller->VertScroll( kControlPageUpPart );
				return TRUE;
				case char_PageDown:
				case char_Space:
					if ( mScroller && mScroller->HasVerticalScrollbar())
						mScroller->VertScroll( kControlPageDownPart );
				return TRUE;
				case char_Home:
					ScrollImageTo( 0, 0, TRUE );
				return TRUE;
				case char_End:
					int32 y;
					y = mImageSize.height - mFrameSize.height;
					if ( y < 0)
						y = 0;
					ScrollImageTo( 0, y, TRUE );
				return TRUE;
			}
		if ( ( modifiers & cmdKey ) == cmdKey )
			switch ( theChar & charCodeMask )
			{
				case char_UpArrow:
					if ( mScroller && mScroller->HasVerticalScrollbar() )
						mScroller->VertScroll( kControlPageUpPart );
				return TRUE;

				case char_DownArrow:
					if ( mScroller && mScroller->HasVerticalScrollbar() )
						mScroller->VertScroll( kControlPageDownPart );
				return TRUE;
			}
		if ( ( modifiers & (controlKey | optionKey ) ) == ( controlKey | optionKey ) )
			switch ( theChar & charCodeMask ) 
			{
				case 'h' - 96:
					if ( mScroller )
					{
						mScroller->ShowScrollbars( FALSE, FALSE );
						return TRUE;
					}
				case 'j' - 96:
					if ( mScroller )
					{
						mScroller->ShowScrollbars( TRUE, TRUE );
						return TRUE;
					}
				case 'k' - 96:
					if ( mScroller )
					{
						mScroller->ShowScrollbars( FALSE, TRUE );
						return TRUE;
					}
				case 'l' - 96:
					if ( mScroller )
					{
						mScroller->ShowScrollbars( TRUE, FALSE );
						return TRUE;
					}
			}
		// Tabbing. We want to shift hyperviews, and not form elements, if we
		// are going backward from the first form element, or forward from the last one
		if (theChar == char_Tab)
		{
			Boolean retVal;

			if (mSubCommanders.GetCount() == 0)	// no tabbing, nothing to switch
				retVal = LCommander::HandleKeyPress(inKeyEvent);
			else
			{
				// ¥¥¥ 98/03/26: Because of the new implementation of LTabGroup in Pro2, we get
				// stuck in the deepest commander hierarchy and can never get back to the
				// toplevel tab group to get to the location bar. This needs to be fixed, but
				// not before 3/31. I'm not sure what the right fix is (pinkerton).
				LCommander *onDutySub = GetTarget();
				if (onDutySub == NULL)
				{
					LCommander	*newTarget;
					mSubCommanders.FetchItemAt(1, newTarget);
					SwitchTarget(newTarget);
					retVal = TRUE;
				}
				else
				{			
					Int32	pos = mSubCommanders.FetchIndexOf(onDutySub);
					Boolean backward = (inKeyEvent.modifiers & shiftKey) != 0;
					if ((pos == mSubCommanders.GetCount() && !backward)	// If we are the last field,
						|| (--pos <= 0 && backward))	//
					// Do not wrap within the view, use the commander
						retVal = LCommander::HandleKeyPress(inKeyEvent);
					else
						retVal = LTabGroup::HandleKeyPress( inKeyEvent );
				}	
			}
			return retVal;
		}
		return LTabGroup::HandleKeyPress( inKeyEvent );
	}
	return handled;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ Callback for cursor handling for new mocha event stuff	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// AdjustCursorRecord
// Used as a Mocha callback structure
// holds all the information needed to call MochaExecuteClickInLink
struct AdjustCursorRecord
{
	char * mMessage;
	AdjustCursorRecord(const char * message)
	{
		if (message)
			mMessage = XP_STRDUP( message );
		else
			mMessage = NULL;
	}
	~AdjustCursorRecord()
	{
		FREEIF( mMessage );
	}
};

// Mocha callback for MochaAdjustCursorRecord
void MochaAdjustCursorCallback(MWContext * pContext,
							   LO_Element * lo_element,
							   int32 lType,
							   void * whatever,
							   ETEventStatus status);
void MochaAdjustCursorCallback(MWContext * pContext,
							   LO_Element * /* lo_element */,
							   int32 /* lType */,
							   void * whatever,
							   ETEventStatus status)
{
	AdjustCursorRecord * p = (AdjustCursorRecord*)whatever;
	if (status == EVENT_CANCEL)
	{
		CNSContext* nsContext = ExtractNSContext(pContext);
		nsContext->SetStatus(p->mMessage);
	}
	delete p;
}

// Mocha callback for MochaAdjustCursorRecord
// Also frees the layout record. Layout record needs to be allocated
// because of a hack for map areas
void MochaAdjustCursorCallbackLayoutFree(MWContext * pContext, 
										LO_Element * lo_element, 
										int32 lType, 
										void * whatever, 
										ETEventStatus status);
void MochaAdjustCursorCallbackLayoutFree(MWContext * pContext, 
										LO_Element * lo_element, 
										int32 lType, 
										void * whatever, 
										ETEventStatus status)
{
	MochaAdjustCursorCallback( pContext, lo_element, lType, whatever, status);
	XP_FREE( lo_element );
}


void CHTMLView::AdjustCursorSelf( Point inPortPt, const EventRecord& inMacEvent )
{
	// if this page is tied-up, display a watch
	if ( !IsEnabled() )
	{
		::SafeSetCursor( watchCursor );
		return;
	}

	//  bail if we did not move, saves time and flickering
	if ( ( inPortPt.h == mOldPoint.h ) && ( inPortPt.v == mOldPoint.v ) )
		return;

	mOldPoint.h = inPortPt.h;
	mOldPoint.v = inPortPt.v;

	Point			localPt = inPortPt;
	SPoint32		firstP;
	
	PortToLocalPoint( localPt );
	LocalToImagePoint(	localPt, firstP );
	if (mCompositor != NULL) {
		CL_Event		event;
		fe_EventStruct	fe_event;
		
		fe_event.portPoint = inPortPt;
		//fe_event.event =  (void *)&inMacEvent;
		fe_event.event.macEvent = inMacEvent; // 1997-02-27 mjc

		event.type = CL_EVENT_MOUSE_MOVE;
		event.fe_event = (void *)&fe_event;
		event.fe_event_size = sizeof(fe_EventStruct); // 1997-02-27 mjc
		event.x = firstP.h;
		event.y = firstP.v;
		event.which = 1;
		event.modifiers = 0;
		
		CL_DispatchEvent(*mCompositor, &event);	
	}
	else
		AdjustCursorSelfForLayer(inPortPt, inMacEvent, NULL, firstP);
}

void CHTMLView::AdjustCursorSelfForLayer( Point inPortPt, const EventRecord& inMacEvent, 
										  CL_Layer *layer, SPoint32 inLayerPt )
{
// With LAYERS, the original method gets the compositor to dispatch
// the event, which is then dealt with (in this method) on a per-layer
// basis.
	
	// find the element the cursor is above, 
	SPoint32		firstP;
	LO_Element* 	element;	
	
    PortToLocalPoint( inPortPt );
	firstP = inLayerPt;

	FocusDraw();								// Debugging only
	
	element = LO_XYToElement( *mContext, firstP.h, firstP.v, layer );

	//
	// Send MouseLeave events for layout elements
	// and areas of client-side image maps.
	//

	if (!CMochaHacks::IsMouseOverElement(element))
		CMochaHacks::SendOutOfElementEvent(*mContext, layer, firstP); // add where parameter - mjc
	
	//
	// If cursor is over blank space, reset and bail
	//
	// pkc (6/5/96) If there is a defaultStatus string, don't clear status area
	if ( element == NULL && mContext->GetDefaultStatus() == NULL )
	{
		CMochaHacks::SetMouseOverElement(NULL);
		CMochaHacks::ResetMochaMouse();
		::SetCursor( &UQDGlobals::GetQDGlobals()->arrow );
		mContext->SetStatus(CStr255::sEmptyString);
		mOldEleID = -1;
		return;
	}
			
	
	//
	// If cursor is over same element as last time and that element
	// is not a client-side image map (sMouseOverMapArea != NULL), then
	// we're done
	//
	if (element
		&& element->lo_any.ele_id == mOldEleID
		&& !CMochaHacks::GetMouseOverMapArea() )
		return;	

	cstring anchor, location;
	CHTMLClickRecord cr(inPortPt, firstP, mContext, element, layer);

	if ( cr.IsAnchor() && !( inMacEvent.modifiers & shiftKey ) )
	{
		// tj (4/29/96): I belive this code forces us to recompute
		// every time in the case of images
		//
		// (5/2/96): Don't set the element to NULL if its an image.
		// We need to absolutely know if we're over the same image
		// the next time through this call.
		//
		// We can use mOldEleID to force a redisplay of status for
		// images (image map coords).
		//
		if ( ( element->type == LO_IMAGE &&
				( element->lo_image.image_attr->attrmask & LO_ATTR_ISMAP )  ) || 
			( cr.mClickKind == eImageIcon ) ) 
			mOldEleID = -1;
		else 
			mOldEleID = element->lo_any.ele_id;
		
		// 97-06-21 pkc -- Also save MWContext along with element because if we mouse out
		// and we switch frames, the mContext when we send the mouseout event is NOT the same
		// one that contains the LO_Element.
		CMochaHacks::SetMouseOverElement(element, *mContext);		
		location = (cr.mClickKind == eImageAnchor) ? cr.mImageURL : cr.mClickURL; 
		//
		// Handle changes in client-side map area
		// 	
		if (!CMochaHacks::IsMouseOverMapArea(cr.mAnchorData))
		{
			// leave the old area
//			if ( CFrontApp::sMouseOverMapArea )
//				LM_SendMouseOutOfAnchor( *mContext, CFrontApp::sMouseOverMapArea );
			CMochaHacks::SendOutOfMapAreaEvent(*mContext, layer, firstP); // add where parameter - mjc				
			CMochaHacks::SetMouseOverMapArea(cr.mAnchorData);
						
			// If the mouseover handler returns true, then 
			// we don't change the location in the status filed
			//
//			if ( CFrontApp::sMouseOverMapArea ) 
//				showLocation = ! LM_SendMouseOverAnchor( *mContext, CFrontApp::sMouseOverMapArea );
			if (CMochaHacks::GetMouseOverMapArea())
			{
				LO_Element * el = XP_NEW_ZAP(LO_Element);	// Need to fake the element, ask chouck for details
				el->type = LO_TEXT;
				el->lo_text.anchor_href = CMochaHacks::GetMouseOverMapArea();
				el->lo_text.text = el->lo_text.anchor_href->anchor; // for js freed element test
				AdjustCursorRecord * r = new AdjustCursorRecord(location);
				// ET_SendEvent now takes a JSEvent struct instead of a int type
				JSEvent* event = XP_NEW_ZAP(JSEvent);
				if (event)
				{
					event->type = EVENT_MOUSEOVER;
					event->x = firstP.h; // layer-relative coordinates - mjc
					event->y = firstP.v;
					event->docx = event->x + CL_GetLayerXOrigin(layer);
					event->docy = event->y + CL_GetLayerYOrigin(layer);
					
					Point screenPt;
					ImageToAvailScreenPoint(firstP, screenPt);
					event->screenx = screenPt.h;
					event->screeny = screenPt.v;
					
					event->layer_id = LO_GetIdFromLayer(*mContext, layer); // 1997-03-02 mjc
					ET_SendEvent( *mContext, el, event, MochaAdjustCursorCallbackLayoutFree, r );
				}
			}
		}
			
		//
		// Only send mouseover for the layout element if the cursor
		// is not over a client-side map area
		//
		if ( !CMochaHacks::GetMouseOverMapArea() )
		{
//			showLocation = !LM_SendMouseOver( *mContext, element );
			AdjustCursorRecord * r = new AdjustCursorRecord(location);
			// ET_SendEvent now takes a JSEvent struct instead of a int type
			JSEvent* event = XP_NEW_ZAP(JSEvent);
			if (event)
			{
				event->type = EVENT_MOUSEOVER;
				event->x = firstP.h; // layer-relative coordinates - mjc
				event->y = firstP.v;
				event->docx = event->x + CL_GetLayerXOrigin(layer);
				event->docy = event->y + CL_GetLayerYOrigin(layer);
				
				Point screenPt;
				ImageToAvailScreenPoint(firstP, screenPt);
				event->screenx = screenPt.h;
				event->screeny = screenPt.v;
				
				event->layer_id = LO_GetIdFromLayer(*mContext, layer); // 1997-03-02 mjc
				ET_SendEvent( *mContext, element, event, MochaAdjustCursorCallback, r );
			}
		}
		
		::SafeSetCursor(curs_Hand);
		return;
	}
	else if ( cr.IsEdge() )
	{
		::SafeSetCursor( (cr.mIsVerticalEdge) ? curs_HoriDrag : curs_VertDrag );
/*	97-06-11 pkc -- what in blue blazes is all this code for?	
		mOldEleID = -1;
		{
			FocusDraw();	// Debugging only
			Rect limitRect, slopRect;
			int axis;
			StRegion rgn;
			
			// Figure out the outline of the region. This should come from layout somehow
			Point topRgn = inPortPt;		
			Point bottomRgn =  inPortPt;
			if ( cr.mIsVerticalEdge )	// Edge is vertical, we can drag horizontally.
			{
				topRgn.v = element->lo_edge.y;
				bottomRgn.v = topRgn.v + element->lo_edge.height;
				topRgn.h = element->lo_edge.x;
				bottomRgn.h = topRgn.h + element->lo_edge.width;
				limitRect.left = cr.mEdgeLowerBound;
				limitRect.right = cr.mEdgeUpperBound;
				limitRect.top = topRgn.v;
				limitRect.bottom = bottomRgn.v;
				slopRect = limitRect;
				axis = hAxisOnly;
			}
			else
			{
				topRgn.h = element->lo_edge.x;
				bottomRgn.h = topRgn.h + element->lo_edge.width;
				topRgn.v = element->lo_edge.y;
				bottomRgn.v = topRgn.v + element->lo_edge.height;
				limitRect.top = cr.mEdgeLowerBound;
				limitRect.bottom = cr.mEdgeUpperBound;
				limitRect.left = topRgn.h;
				limitRect.right = bottomRgn.h;
				slopRect = limitRect;
				axis = vAxisOnly;
			}
			OpenRgn();
//			ShowPen();
			MoveTo(topRgn.h, topRgn.v);
			LineTo(bottomRgn.h, topRgn.v);	// Framing of the rect
			LineTo(bottomRgn.h, bottomRgn.v);
			LineTo(topRgn.h, bottomRgn.v);
			LineTo(topRgn.h, topRgn.v);
			CloseRgn(rgn.mRgn);
//			FillRgn(rgn.mRgn, &qd.black);
//			FrameRect(&limitRect);
		}*/
		return;
	}
	
	
	//
	// The cursor is over blank space
	//
	
	// If we were previously over a map area, leave the old area

	if (CMochaHacks::GetMouseOverMapArea() != NULL) {
//		LM_SendMouseOutOfAnchor(*mContext, CFrontApp::sMouseOverMapArea);
		LO_Element * el = XP_NEW_ZAP(LO_Element);	// Need to fake the element, ask chouck for details
		el->type = LO_TEXT;
		el->lo_text.anchor_href = CMochaHacks::GetMouseOverMapArea();
		if (el->lo_text.anchor_href->anchor)
			el->lo_text.text = el->lo_text.anchor_href->anchor; // to pass js freed element check
		AdjustCursorRecord * r = new AdjustCursorRecord(location);
		// ET_SendEvent now takes a JSEvent struct instead of a int type
		JSEvent* event = XP_NEW_ZAP(JSEvent);
		if (event)
		{
			event->type = EVENT_MOUSEOUT;
			event->x = firstP.h; // layer-relative coordinates - mjc
			event->y = firstP.v;
			event->docx = event->x + CL_GetLayerXOrigin(layer);
			event->docy = event->y + CL_GetLayerYOrigin(layer);
			
			Point screenPt;
			ImageToAvailScreenPoint(firstP, screenPt);
			event->screenx = screenPt.h;
			event->screeny = screenPt.v;
			event->layer_id = LO_GetIdFromLayer(*mContext, layer); // 1997-03-02 mjc
			ET_SendEvent( *mContext, el, event, MochaAdjustCursorCallbackLayoutFree, r );
		}
		CMochaHacks::SetMouseOverMapArea(NULL);
	}
	
	::SetCursor( &UQDGlobals::GetQDGlobals()->arrow );
	mOldEleID = -1;
	mContext->SetStatus( (	mContext->GetDefaultStatus() != NULL) ? 
							mContext->GetDefaultStatus() :
							CStr255::sEmptyString );
}







// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- DRAG AND DROP ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ




void CHTMLView::DoDragSendData(FlavorType inFlavor,
								ItemReference inItemRef,
								DragReference inDragRef)
{
	OSErr			theErr;
	cstring			theUrl;
		
	// Get the URL of the thing we're dragging...
	if (mDragElement->type == LO_IMAGE)
	{
		theUrl = GetURLFromImageElement(mContext, (LO_ImageStruct*) mDragElement);		// do this so we can get name of non-anchor images
	}
	else
	{
		PA_Block anchor;
		XP_ASSERT(mDragElement->type == LO_TEXT);
	 	anchor = mDragElement->lo_text.anchor_href->anchor;
	 	PA_LOCK (theUrl, char*, anchor);
		PA_UNLOCK(anchor);	
	}
 	
 		// Now send the data	
	switch (inFlavor)
	{
			// Just send the URL text
		case 'TEXT':
		{
			theErr = ::SetDragItemFlavorData(inDragRef, inItemRef, inFlavor, theUrl, strlen(theUrl), 0);
			ThrowIfOSErr_ (theErr);
			break;
		}
		 
			// Send the image as a PICT
		case 'PICT':
		{
			Assert_(mDragElement->type == LO_IMAGE);
			PicHandle thePicture = ConvertImageElementToPICT((LO_ImageStruct_struct*) mDragElement);
			ThrowIfNULL_(thePicture);
			
				{
					StHandleLocker theLock((Handle)thePicture);
					theErr = ::SetDragItemFlavorData(inDragRef, inItemRef, inFlavor, *thePicture, GetHandleSize((Handle)thePicture), 0);
					ThrowIfOSErr_(theErr);
				}
						
			::KillPicture(thePicture);
			break;
		}
		
		case emBookmarkFileDrag:
		{
				// Get the target drop location
			AEDesc dropLocation;
			
			theErr = ::GetDropLocation(inDragRef, &dropLocation);
			//ThrowIfOSErr_(theErr);	
			if (theErr != noErr)
				return;
			
				// Get the directory ID and volume reference number from the drop location
			SInt16 	volume;
			SInt32 	directory;
			
			theErr = GetDropLocationDirectory(&dropLocation, &directory, &volume);
			//ThrowIfOSErr_(theErr);	
		
				// Ok, this is a hack, and here's why:  This flavor type is sent with the FlavorFlag 'flavorSenderTranslated' which
				// means that this send data routine will get called whenever someone accepts this flavor.  The problem is that 
				// it is also called whenever someone calls GetFlavorDataSize().  This routine assumes that the drop location is
				// something HFS related, but it's perfectly valid for something to query the data size, and not be a HFS
				// derrivative (like the text widget for example).
				// So, if the coercion to HFS thingy fails, then we just punt to the textual representation.
			if (theErr == errAECoercionFail)
			{
				theErr = ::SetDragItemFlavorData(inDragRef, inItemRef, inFlavor, theUrl, strlen(theUrl), 0);
				return;
			}
	
			if (theErr != noErr)
				return;
			if (mDragElement->type == LO_IMAGE && theUrl == "")
				break;

			URL_Struct*		request;		
			request = NET_CreateURLStruct(theUrl, NET_DONT_RELOAD);
				// Combine with the unique name to make an FSSpec to the new file
			FSSpec		prototypeFilespec;
			FSSpec		locationSpec;
			prototypeFilespec.vRefNum = volume;
			prototypeFilespec.parID = directory;
			CStr31 filename;
			Boolean isMailAttachment = false;
#ifdef MOZ_MAIL_NEWS
			isMailAttachment = XP_STRSTR( request->address , "?part=") || XP_STRSTR( request->address, "&part=");
			if ( isMailAttachment ) 
			{
				CHTMLView::GetDefaultFileNameForSaveAs( request, filename);
			}
			else
#endif // MOZ_MAIL_NEWS
			{
				filename = CFileMgr::FileNameFromURL( request->address );
				//GetDefaultFileNameForSaveAs( request , filename );
			}
			
			//theErr = CFileMgr::NewFileSpecFromURLStruct(filename, prototypeFilespec, locationSpec);
			theErr = CFileMgr::UniqueFileSpec( prototypeFilespec,filename, locationSpec );
			if (theErr && theErr != fnfErr) // need a unique name, so we want fnfErr!
				ThrowIfOSErr_(theErr);
			
				// Set the flavor data to our emBookmarkFileDrag flavor with an FSSpec to the new file.
			theErr = ::SetDragItemFlavorData (inDragRef, inItemRef, inFlavor, &locationSpec, sizeof(FSSpec), 0);
			ThrowIfOSErr_(theErr);
		
			
			XP_MEMSET(&request->savedData, 0, sizeof(SHIST_SavedData));
			CURLDispatcher::DispatchToStorage(request, locationSpec, FO_SAVE_AS, isMailAttachment);
			// ¥¥¥ both blocks of the if/then/else above call break, so does this get called?  96-12-17 deeje
			//CFileMgr::UpdateFinderDisplay(locationSpec); 
		}
			
		case emBookmarkDrag:
		{
			cstring urlAndTitle(theUrl);
			if (mDragElement->type == LO_IMAGE)
				urlAndTitle += "\r[Image]";
			else if (mDragElement->type == LO_TEXT)
				{
				urlAndTitle += "\r";
				cstring title;
				PA_LOCK(title, char *, mDragElement->lo_text.text);
				PA_UNLOCK(mDragElement->lo_text.text);
				urlAndTitle += title;
				}			
				
			theErr = ::SetDragItemFlavorData(inDragRef, inItemRef, inFlavor, urlAndTitle, strlen(urlAndTitle), 0);
			break;	
		}
				
		default:
		{
			Throw_(cantGetFlavorErr); // caught by PP handler
			break;
		}
	}		

}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- TIMER URL ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::SetTimerURL(Uint32 inSeconds, const char* inURL)
{
	if (inURL)
		mTimerURLString = XP_STRDUP(inURL);
	else
		return;
	mTimerURLFireTime = ::TickCount() + inSeconds * 60;
	StartRepeating();
}

void CHTMLView::ClearTimerURL(void)
{
	if (mTimerURLString)
		XP_FREE(mTimerURLString);
	mTimerURLString = NULL;
	StopRepeating();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- DEFERRED LOADING ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::PostDeferredImage(const char* inImageURL)
{
	Assert_(inImageURL != NULL);

	LO_SetForceLoadImage((char *)inImageURL, FALSE);
	string theURL(inImageURL);
	mImageQueue.push_back(theURL);
	mContext->Repaginate();
}

Boolean CHTMLView::IsImageInDeferredQueue(const char* inImageURL) const
{
	Boolean bFound = false;
	if (mImageQueue.size() > 0)
		{
		vector<string>::const_iterator theIter = mImageQueue.begin();
		while (theIter != mImageQueue.end())
			{
			if (*theIter == inImageURL)
				{
				bFound = true;
				break;
				}
			++theIter;
			}
		}
	
	return bFound;
}

void CHTMLView::ClearDeferredImageQueue(void)
{
	mImageQueue.erase(mImageQueue.begin(), mImageQueue.end());
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- URL DISPATCHING ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// 97-05-30 pkc -- These methods are here so that subclasses can alter the
// CURLDispatchInfo struct as necessary before we call CURLDispatcher::DispatchURL

void CHTMLView::DispatchURL(
	URL_Struct* inURLStruct,
	CNSContext* inTargetContext,
	Boolean inDelay,
	Boolean inForceCreate,
	FO_Present_Types inOutputFormat)
{
	CURLDispatchInfo* info =
		new CURLDispatchInfo(inURLStruct, inTargetContext, inOutputFormat, inDelay, inForceCreate);
	DispatchURL(info);
}

void CHTMLView::DispatchURL(CURLDispatchInfo* inDispatchInfo)
{
	CURLDispatcher::DispatchURL(inDispatchInfo);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- CONTEXT CALLBACK IMPLEMENTATION ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// XXX CALLBACK
void CHTMLView::LayoutNewDocument(
	URL_Struct*			/* inURL */,
	Int32*				ioWidth,
	Int32*				ioHeight,
	Int32*				ioMarginWidth,
	Int32*				ioMarginHeight)
{
	if (IsRootHTMLView())
		{
		//Assert_(mScroller != NULL); Don't HAVE to have scrollers, as long as we check for 'em.
		if (mScroller)
			{
			mScroller->ResetExpansion();
			mScroller->AdjustHyperViewBounds();
			}
		}

	// clear these so we don't send JavaScript mouseover/exit 
	// events for deleted elements
	// ¥¥¥ FIX ME!!!
	CMochaHacks::ResetMochaMouse();
	
	// FIX ME!!! determine why we need this here	
	// Answer:
	// Because the we may load a docuemnt in different CharSet (eg. Japanese, Korean )
	//		ftang
	SetFontInfo();

	mHasGridCells = FALSE;
	*ioWidth = *ioHeight = 0;
	
	if (*ioMarginWidth == 0)
		{
		*ioMarginWidth = 8;
		*ioMarginHeight = 8;
		}
	
	Assert_(mContext != NULL);
	if (IsRootHTMLView())
		SetScrollMode(mDefaultScrollMode);

	// Clear the previous page
	ScrollImageTo(0, 0, false);
	if (mEraseBackground)
		ClearView();
	
	Rect theFrame;
	CalcLocalFrameRect(theFrame);

	*ioWidth = theFrame.right;
	*ioHeight = theFrame.bottom;

	// Resize the images from the _current_ document size to the _preferred_ size.
	// Resize directly, because SetDocDimension does not decrease the size for main view
	ResizeImageTo(*ioWidth, *ioHeight, false);
	
	// 97-06-18 pkc -- Fix bug #70661. Don't do this margin altering code if frame width
	// and/or height is zero.
	if (theFrame.right - theFrame.left > 0)
		{
		if (*ioMarginWidth > (theFrame.right / 2 ))
			*ioMarginWidth = MAX( theFrame.right / 2 - 50, 0);
		}
	if (theFrame.bottom - theFrame.top > 0)
		{
		if (*ioMarginHeight > (theFrame.bottom / 2 ))
			*ioMarginHeight = MAX(theFrame.bottom / 2 -50, 0);
		}

	// If the vertical scrollbar might be shown,
	// tell layout that we already have it, so that it does
	// the correct wrapping of text.
	// If we do not do this, when vertical scrollbar only is
	// shown, some text might be hidden
	if (GetScrollMode() == LO_SCROLL_AUTO)
		*ioWidth -= 15;
}


// XXX CALLBACK

void CHTMLView::ClearView(
	int 					/* inWhich */)
{
	ClearBackground();
	StopRepeating();
}


void CHTMLView::InstallBackgroundColor(void)
{
	// ¥ install the user's default solid background color & pattern
	mBackgroundColor = CPrefs::GetColor(CPrefs::TextBkgnd);
}

void CHTMLView::GetDefaultBackgroundColor(LO_Color* outColor) const
{
	*outColor = UGraphics::MakeLOColor(mBackgroundColor);
}

void CHTMLView::SetWindowBackgroundColor()
{
	// Danger!  Port is not necessarily a window.
	// To fix, compare LWindow::FetchWindowObject result versus LWindow
	// found by going up view hierarchy.
	LWindow* portWindow = LWindow::FetchWindowObject(GetMacPort());
	LWindow* ourWindow = NULL;
	LView* view = this;

	while( view != NULL )
	{
		view = view->GetSuperView();
		if (view)
		{
			ourWindow = dynamic_cast<LWindow*>(view);
			if (ourWindow)
				break;
		}
	}

	if (portWindow == ourWindow)
		UGraphics::SetWindowColor(GetMacPort(), wContentColor, mBackgroundColor);
}

void CHTMLView::ClearBackground(void)
{
	if (!FocusDraw())
		return;

	// ¥ dispose of the current background
	mBackgroundImage = NULL;

	// ¥ install the user's default solid background color & pattern
	InstallBackgroundColor();

	// Danger!  Port is not necessarily a window.
//	if (LWindow::FetchWindowObject(GetMacPort()))
//		UGraphics::SetWindowColor(GetMacPort(), wContentColor, mBackgroundColor);
	SetWindowBackgroundColor();
	::RGBBackColor(&mBackgroundColor);
		
	// erase the widget to the user's default background
	
	Rect theFrame;
	if (CalcLocalFrameRect(theFrame))
		DrawBackground(theFrame, nil);
}

void CHTMLView::DrawBackground(
	const Rect& 			inArea,
	LO_ImageStruct*			inBackdrop)
{
	if (!FocusDraw())
		return;

	// We're using this to denote that we _do_ have an outline
	Rect theFrame; //, theEraseArea;
	CalcLocalFrameRect(theFrame);

	::SectRect(&theFrame, &inArea, &theFrame);
	
	//
	// Get our default clip from the port so that we use the correct clip
	// from layers
	//
	StRegion theUpdateMask;
	::GetClip(theUpdateMask);

	//
	// Never erase the plug-in area -- the plug-ins
	// will take care of that for us, and we donÕt
	// want to cause flicker with redundant drawing.
	//
	StRegion thePluginMask;
	::SetEmptyRgn(thePluginMask);
	CalcPluginMask(thePluginMask);

	if (!::EmptyRgn(thePluginMask))
		{
		Point	theLocalOffset = { 0, 0 };
		PortToLocalPoint(theLocalOffset);
		::OffsetRgn(thePluginMask, theLocalOffset.h, theLocalOffset.v);
		::DiffRgn(theUpdateMask, thePluginMask, theUpdateMask);
		}

	StClipRgnState theClipState(theUpdateMask);
	DrawBackgroundSelf(theFrame, inBackdrop);
}


void CHTMLView::DrawBackgroundSelf(
	const Rect& 			inArea,
	LO_ImageStruct*			inBackdrop)
{
	LO_ImageStruct *backdrop;

	if (inBackdrop)
		backdrop = inBackdrop;
	else if (mBackgroundImage)
		backdrop = mBackgroundImage;
	else
		backdrop = NULL;
		
	// ¥ if we can't draw the image then fall back on the pattern/solid
	if ( backdrop )
		{
		}
	else
		::EraseRect(&inArea);
}

void CHTMLView::EraseBackground(
	int						/* inLocation */,
	Int32					inX,
	Int32					inY,
	Uint32					inWidth,
	Uint32					inHeight,
	LO_Color*				inColor)
{
	if (!FocusDraw())
		return;

	// Convert draw rect from image coordinate system to local
	SPoint32 		imageTopLeft;
	imageTopLeft.h = inX;
	imageTopLeft.v = inY;

	SPoint32		imageBottomRight;
	imageBottomRight.h = inX + inWidth;
	imageBottomRight.v = inY + inHeight;

	Rect theLocalEraseArea;
	ImageToLocalPoint(imageTopLeft, topLeft(theLocalEraseArea));
	ImageToLocalPoint(imageBottomRight, botRight(theLocalEraseArea));
	
	if (inColor != NULL)
		{
		// Make sure we don't erase the scrollbars.
		// NOTE: don't call StClipRgnState() here:
		// it messes up with the text selection.
		Rect frame;
		CalcLocalFrameRect(frame);
//		StClipRgnState tempClip(frame);
		if (theLocalEraseArea.right > frame.right)
			theLocalEraseArea.right = frame.right;
		if (theLocalEraseArea.bottom > frame.bottom)
			theLocalEraseArea.bottom = frame.bottom;

		RGBColor theBackColor = UGraphics::MakeRGBColor(inColor->red, inColor->green, inColor->blue);
		::RGBBackColor(&theBackColor);
		::EraseRect(&theLocalEraseArea);
		}
	else
		DrawBackground(theLocalEraseArea);
}

int CHTMLView::SetColormap(
	IL_IRGB*				/* inMap */,
	int						/* inRequested */)
{
	return 0;
}

void CHTMLView::SetBackgroundColor(
	Uint8 					inRed,
	Uint8					inGreen,
	Uint8 					inBlue)
{
	RGBColor theBackground = UGraphics::MakeRGBColor(inRed, inGreen, inBlue);
	
	GrafPtr thePort = GetMacPort();
	if (thePort != NULL && mContext != NULL && IsRootHTMLView()
		&& LWindow::FetchWindowObject(GetMacPort()))
		{
		UGraphics::SetWindowColor(thePort, wContentColor, theBackground);
		}

	if (!UGraphics::EqualColor(mBackgroundColor, theBackground))
		{
		mBackgroundColor = theBackground;
		Refresh();
		}
	
	/* now, call the image code to setup this new background color in the color map */
	SetImageContextBackgroundColor ( *GetContext(), inRed, inGreen, inBlue );
}

void CHTMLView::SetBackgroundImage(
	LO_ImageStruct* 		inImageStruct,
	Boolean 				inRefresh)
{
	mBackgroundImage = inImageStruct;
	
	if (inRefresh)
		{
		Rect theFrame;
		CalcLocalFrameRect(theFrame);
		DrawBackground(theFrame);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	CalcPluginMask
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//	The following code calculates the union of all exposed plugin areas.  The
// 	result is added to ioPluginRgn.  It is assumed that ioPluginRgn is a
//	valid region (already constructed with NewRgn()).  The region is port
//	relative.
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// REGION IS BASED ON PORT COORDINATES

void CHTMLView::CalcPluginMask(RgnHandle ioPluginRgn)
{
	Assert_(ioPluginRgn != NULL);

	if (mSubPanes.GetCount() == 0)								// Any subpanes?
		return;

	LArrayIterator iter(mSubPanes);
	LPane* pane = NULL;
	while (iter.Next(&pane ))									// Check each subpane
		{
		if ((pane->GetPaneID() == CPluginView::class_ID)	||
			(pane->GetPaneID() == 'java'))		// Is it a plug-in or a java pane?
			{
			CPluginView* plugin = (CPluginView*)pane;
			if (plugin->IsPositioned() &&							// Position is valid?
				NPL_IsEmbedWindowed(plugin->GetNPEmbeddedApp()))
				{
				Rect theRevealedFrame;
				plugin->GetRevealedRect(theRevealedFrame);
				if (!EmptyRect(&theRevealedFrame))				// Plug-in visible in page?
					{
					StRegion thePluginFrameRgn(theRevealedFrame);
					::UnionRgn(ioPluginRgn, thePluginFrameRgn, ioPluginRgn);
					}
				}
			}
		}
}

Boolean CHTMLView::IsGrowCachingEnabled() const
		/*
			...only derived classes with special needs would need better control than this.
			See |CEditView|.
		*/
	{
		return true;
	}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	SetDocDimension
//
//	This evidently gets called a bazillion times from within layout.  We need
//	to cache the calls so that the image is not resized all the time.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// XXX CALLBACK

void CHTMLView::SetDocDimension(
	int 					/* inLocation */,
	Int32 					inWidth,
	Int32 					inHeight)
{
	SDimension32 oldImageSize;
	GetImageSize(oldImageSize);

	SDimension32 newImageSize;
	newImageSize.width	= inWidth;
	newImageSize.height	= inHeight;

		// If we can cache `grows' to enhance performance...
	if ( IsGrowCachingEnabled() )
		{
				// ...then we only need to grow if we ran out of space...
			if ( (oldImageSize.width < inWidth) || (oldImageSize.height < inHeight) )
				{
					SDimension16 curFrameSize;
					GetFrameSize(curFrameSize);
						// Warning: in some funky Java cases, |curFrameSize| can be 0,0.


						// Start doubling from which ever is greater, the frame height or the old image height.
					newImageSize.height = ((curFrameSize.height	>= oldImageSize.height)	? curFrameSize.height	: oldImageSize.height);

						// Ensure that doubling the height will terminate the doubling-loop.
					if ( newImageSize.height < 1 )
						newImageSize.height = 1;

						// Now, double the current height until it's greater than the requested height
					do
						newImageSize.height += newImageSize.height;
					while ( newImageSize.height < inHeight );
						// That ought to hold 'em for a while...
				}
			else
					// ...else, we'll grow later, what we have now is good enough.
				newImageSize = oldImageSize;
		}


		// Resize the image, if we need to...
	if ( (newImageSize.height != oldImageSize.height) || (newImageSize.width != oldImageSize.width) )
		ResizeImageTo(newImageSize.width, newImageSize.height, true);
		

		// If we didn't exactly satisfy the request, then we'll have to later...
	if ( (mPendingDocDimension_IsValid = ((newImageSize.height != inHeight) || (newImageSize.width != inWidth))) == true )
		{
			mPendingDocDimension.width	= inWidth;
			mPendingDocDimension.height	= inHeight;
		}
}


void CHTMLView::FlushPendingDocResize(void)
{
	if ( mPendingDocDimension_IsValid )
		{
			mPendingDocDimension_IsValid = false;

			SDimension32 imageSize;
			GetImageSize(imageSize);

			if ( (imageSize.width != mPendingDocDimension.width) || (imageSize.height != mPendingDocDimension.height) )
				ResizeImageTo(mPendingDocDimension.width, mPendingDocDimension.height, true);
		}
}


// XXX CALLBACK

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	SetDocPosition
//
// Scrolls the image to (inX, inY), limited to the image size minus the frame size. The x position is
// left unchanged if it is already visible.
//
// inLocation: ignored
// inX, inY: new position. Negative values will be converted to zero.
// inScrollEvenIfVisible: if true, will cause the html view to scroll even if the x position is already
// visible (defaults to false, which is the historical behavior).
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void CHTMLView::SetDocPosition(
	int 					/* inLocation */,
	Int32 					inX,
	Int32 					inY,
	Boolean					inScrollEvenIfVisible)
{
	//
	// If we're displaying a full-page plug-in, ignore the
	// request to position the document.  When displaying
	// a full-page plug-in, we don't show scroll bars and
	// all positioning is handled by the plug-in itself.
	//
	if (mContext->HasFullPagePlugin())
		return;	

	// Make sure our size is not our of sync, flush any pending resize
	FlushPendingDocResize();

	//Ê¥Êlocation is the variable affecting which view are we in 
	//		Make sure that the view is big enough so that we do not get
	//		auto-cropped with PPlant when we auto-scroll

	SDimension16 theFrameSize;
	GetFrameSize(theFrameSize);
	
	SPoint32 theFrameLocation;
	GetFrameLocation(theFrameLocation);
	
	SDimension32 theImageSize;
	GetImageSize(theImageSize);
	
	SPoint32 theImageLocation;
	GetImageLocation(theImageLocation);

	theImageLocation.h -= theFrameLocation.h;
	theImageLocation.v -= theFrameLocation.v;

	// ¥ÊCalculate proper position.
	
	Int32 scrollToX = inX;
	Int32 scrollToY = inY;

	if ((scrollToY + theFrameSize.height) > theImageSize.height)
	{
		scrollToY = theImageSize.height - theFrameSize.height;
	}
	
	// rebracket this test so it's done all the time (not only when we exceed image height)  - mjc 97-9-12
	// Sometimes the image height is 0 and scrollToY goes negative. It
	// seems stupid that image height should ever be 0 - oh well.
	if (scrollToY < 0)
	{
		scrollToY = 0;
	}

	// If x is visible, do not change it.
	if (!inScrollEvenIfVisible && (inX >= theImageLocation.h) && (inX + 10 <= (theImageLocation.h + theFrameSize.width )))
	{
		scrollToX = theImageLocation.h;
	}
	
	// limit the x position as we do with the y position - mjc 97-9-12
	scrollToX = MIN(scrollToX, theImageSize.width - theFrameSize.width);
	if (scrollToX < 0) scrollToX = 0;
		
	ScrollImageTo(scrollToX, scrollToY, true);
}

// XXX CALLBACK

void CHTMLView::GetDocPosition(
	int 					/* inLocation */,
	Int32*					outX,
	Int32*					outY)
{
	// Make sure our size is not our of sync, flush any pending resize
	FlushPendingDocResize();

	SPoint32 theScrollPosition;
	GetScrollPosition(theScrollPosition);
	*outX = theScrollPosition.h;
	*outY = theScrollPosition.v;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int CHTMLView::GetTextInfo(
	LO_TextStruct*			inText,
	LO_TextInfo*			outTextInfo)
{
	Assert_(inText != NULL);
	Assert_(outTextInfo != NULL);
	Assert_(mContext != NULL);
		
	LO_TextAttr* theTextAttr = inText->text_attr;

	Assert_(theTextAttr != NULL);
	if (theTextAttr == NULL)
		return 1;
		

// FIX ME!!! need to optimize the getting of the port (cache it)
// No, don't. This is open to error, and the amount of time saved is minimal.

	GrafPtr theSavePort = NULL;	
	if (GetCachedPort() != qd.thePort)
		{
		theSavePort = qd.thePort;
		::SetPort(GetCachedPort());
		}

	{
		HyperStyle theStyle(*mContext, &mCharSet, theTextAttr, true);
		theStyle.Apply();
		theStyle.GetFontInfo();

		outTextInfo->ascent = theStyle.fFontInfo.ascent;	
		outTextInfo->descent = theStyle.fFontInfo.descent;
		outTextInfo->lbearing = 0;
		
		char* theText;
		PA_LOCK(theText, char*, inText->text);
		
		// ¥Êmeasure the text
		Int16 theWidth = theStyle.TextWidth(theText, 0, inText->text_len);

	// **** Don't know which of these is better; the first only works for native single byte scritps
	// The second will always work, but produces different results...
	// Probably need to Add a function to HyperStyle to handle this...	

	/*
		if (theTextAttr->fontmask & LO_FONT_ITALIC )
			outTextInfo->rbearing = theWidth + (::CharWidth(theText[inText->text_len - 1]) / 2);
		else
			outTextInfo->rbearing = 0;
	*/
		if (theTextAttr->fontmask & LO_FONT_ITALIC )
			outTextInfo->rbearing = theWidth + (theStyle.fFontInfo.widMax / 2);
		else
			outTextInfo->rbearing = 0;
		
		PA_UNLOCK(inText->text);

		outTextInfo->max_width = theWidth;
	}	//so that theStyle goes out of scope and resets the text traits in the correct port
	
	if (theSavePort != NULL)
		::SetPort(theSavePort);
	
	return 1;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	MeasureText
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int CHTMLView::MeasureText(
	LO_TextStruct*			inText,
	short*					outCharLocs)
{
	Assert_(inText != NULL);
	Assert_(outCharLocs != NULL);
	Assert_(mContext != NULL);

	LO_TextAttr* theTextAttr = inText->text_attr;

	Assert_(theTextAttr != NULL);
	if (theTextAttr == NULL)
		return 0;

	// FIX ME!!! need to optimize the getting of the port (cache it)
	GrafPtr theSavePort = NULL;	
	if (GetCachedPort() != qd.thePort)
		{
		theSavePort = qd.thePort;
		::SetPort(GetCachedPort());
		}

	Int16 measuredBytes = 0;
	
	{
		HyperStyle theStyle(*mContext, &mCharSet, theTextAttr, true);
		theStyle.Apply();

		// FIX ME!!! we only work with Native font references for now
		CNativeFontReference* nativeFontReference =
			dynamic_cast<CNativeFontReference*>(theStyle.fFontReference);
		if (nativeFontReference != NULL)
		{
			// measure the text
			measuredBytes = nativeFontReference->MeasureText(
								(char*)inText->text, 0, inText->text_len, outCharLocs);
		}
	}	//so that theStyle goes out of scope and resets the text traits in the correct port
	
	if (theSavePort != NULL)
		::SetPort(theSavePort);
	
	return measuredBytes;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::GetTextFrame(
	LO_TextStruct*			inTextStruct,
	Int32					inStartPos,
	Int32					inEndPos,
	XP_Rect*				outFrame)
{
	Assert_(inTextStruct != NULL);
	Assert_(outFrame != NULL);
	
	GrafPtr theSavePort = NULL;	
	if (GetCachedPort() != qd.thePort)
	{
		theSavePort = qd.thePort;
		::SetPort(GetCachedPort());
	}
	
	{
		LO_TextAttr* theTextAttr = inTextStruct->text_attr;
		HyperStyle theStyle(*mContext, &mCharSet, theTextAttr, true, inTextStruct);

		Point theLocalTopLeft = { 0, 0 };
		
		char* theText;
		PA_LOCK(theText, char*, inTextStruct->text);
		Rect theLocalFrame = theStyle.InvalBackground(theLocalTopLeft, theText, inStartPos, inEndPos, false);
		PA_UNLOCK(inTextStruct->text);
		
		// Convert to layer coordinates
		outFrame->left = theLocalFrame.left + inTextStruct->x + inTextStruct->x_offset;
		outFrame->top = theLocalFrame.top + inTextStruct->y + inTextStruct->y_offset;
		outFrame->right = theLocalFrame.right + inTextStruct->x + inTextStruct->x_offset;
		outFrame->bottom = theLocalFrame.bottom + inTextStruct->y + inTextStruct->y_offset;
	} //so that theStyle goes out of scope and resets the text traits in the correct port
	
	if (theSavePort != NULL)
		::SetPort(theSavePort);
} 

void CHTMLView::DisplaySubtext(
	int 					/* inLocation */,
	LO_TextStruct*			inText,
	Int32 					inStartPos,
	Int32					inEndPos,
	XP_Bool 				inNeedBG)
{
	// ¥ if we can't focus, don't do anything
	if (!FocusDraw())
		return;

	// ¥Êif we're not visible, don't do anything
	Rect theTextFrame;
	if (!CalcElementPosition((LO_Element*)inText, theTextFrame))
		return;
	
	LO_TextAttr* theTextAttr = inText->text_attr;
	
// FIX ME!!! this needs to do the right thing in the editor
//	Boolean bBlinks = (attr->attrmask & LO_ATTR_BLINK) && (!EDT_IS_EDITOR(*mContext));			// no blinking in the editor because of problems with moving text which is blinking
		
		
	HyperStyle style(*mContext, &mCharSet, theTextAttr, false, inText);

	char *theTextPtr;
	PA_LOCK(theTextPtr, char*, inText->text);
	
	if (inNeedBG)			// ¥Êerase the background
		{
		Rect theInvalRect = style.InvalBackground( topLeft(theTextFrame), theTextPtr, inStartPos, inEndPos, false);
		
		if ( theTextAttr->no_background )
			{
			StClipRgnState theClipSaver;
			theClipSaver.ClipToIntersection(theInvalRect);
			DrawBackground(theInvalRect);
			}
		else
			{
			::PenPat(&qd.black );
			UGraphics::SetIfColor(
				UGraphics::MakeRGBColor(theTextAttr->bg.red,
										theTextAttr->bg.green,
										theTextAttr->bg.blue));
										
			::FillRect(&theInvalRect, &qd.black);
			}
		}

	style.DrawText(topLeft(theTextFrame), theTextPtr, inStartPos, inEndPos);

	PA_UNLOCK(inText->text);
}

void CHTMLView::DisplayText(
	int 					inLocation,
	LO_TextStruct*			inText,
	XP_Bool 				inNeedBG)
{
	 DisplaySubtext(inLocation, inText, 0, inText->text_len - 1, inNeedBG);
}

void CHTMLView::DisplayLineFeed(
	int 					/* inLocation */,
	LO_LinefeedStruct*		/* inLinefeedStruct */,
	XP_Bool 				/* inNeedBG */)
{
#ifdef LAYERS
	
#else
	Rect frame;
	Boolean		doDraw;

	// BUGBUG LAYERS: Linefeed drawing causes problems with layers (the problem
	// is not in FE code - the selection code needs to do the same special casing
	// it did for text in the case of line feeds and hrules). The temporary fix is
	// to not draw linefeeds.

	if ( !FocusDraw() )
		return;

	if ( needBg )
	{
		doDraw = CalcElementPosition( (LO_Element*)lineFeed, frame );

		if ( !doDraw )
			return;
			
		// ¥Êfill the background in the color given by layout	
		if ( lineFeed->text_attr->no_background == FALSE )
		{
			UGraphics::SetIfColor(
			UGraphics::MakeRGBColor( 	lineFeed->text_attr->bg.red,
									 	lineFeed->text_attr->bg.green,
										lineFeed->text_attr->bg.blue ) );
	
			::FillRect( &frame, &qd.black );
		}
		else
			this->DrawBackground( frame );
	}

	Boolean selected = lineFeed->ele_attrmask & LO_ELE_SELECTED;
		
	if ( selected )
	{
		if ( CalcElementPosition( (LO_Element*)lineFeed, frame ) )
		{
			RGBColor		color;
			LMGetHiliteRGB( &color );
			
			::RGBBackColor( &color );
			::EraseRect( &frame );
		}
	}

#endif
}

void CHTMLView::DisplayHR(
	int 					/* inLocation */,
	LO_HorizRuleStruct*		inRuleStruct)
{
	if (!FocusDraw())
		return;

	Rect theFrame;
	if (!CalcElementPosition( (LO_Element*)inRuleStruct, theFrame))
		return;
	
// FIX ME!!! remove lame drawing
	if (((inRuleStruct->ele_attrmask & LO_ELE_SHADED)== 0) || ( theFrame.top + 1 >= theFrame.bottom ) )		// No shading, or 1 pixel rect
		{
		UGraphics::SetFore(CPrefs::Black);
		FillRect(&theFrame, &UQDGlobals::GetQDGlobals()->black);
		}
	else
		UGraphics::FrameRectShaded(theFrame, TRUE);
}

void CHTMLView::DisplayBullet(
	int 					/* inLocation */,
	LO_BullettStruct*		inBulletStruct)
{
	if (!FocusDraw())
		return;
	
	Rect theFrame;
	if (!CalcElementPosition((LO_Element*)inBulletStruct, theFrame))
		return;
	
	if (inBulletStruct->text_attr)
		UGraphics::SetIfColor(
		UGraphics::MakeRGBColor( inBulletStruct->text_attr->fg.red,
								 inBulletStruct->text_attr->fg.green,
								 inBulletStruct->text_attr->fg.blue ));
	else
		UGraphics::SetFore(CPrefs::Black);

	switch ( inBulletStruct->bullet_type )
		{
		case BULLET_ROUND:
			::PenPat(&qd.black);
			::FrameOval(&theFrame);
			break;
			
		case BULLET_SQUARE:
			::PenPat(&qd.black);
			::FrameRect(&theFrame);
			break;
			
		case BULLET_BASIC:
			::FillOval(&theFrame, &UQDGlobals::GetQDGlobals()->black);
			break;
			
		case BULLET_MQUOTE:
			::PenPat(&qd.black);
			::MoveTo(theFrame.left, theFrame.top);
			::LineTo(theFrame.left, theFrame.bottom);
			break;
		
		default:		// Should not happen
			::MoveTo(theFrame.left, theFrame.bottom);
			::DrawChar('?');
			break;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::GetEmbedSize(
	LO_EmbedStruct*			inEmbedStruct,
	NET_ReloadMethod		/* inReloadMethod */)
{
	if (inEmbedStruct->FE_Data == NULL)		// Creating plugin from scratch
	{
		Try_
		{
			NPEmbeddedApp* app = NPL_EmbedCreate(*mContext, inEmbedStruct);
			ThrowIfNil_(app);
			
			// XXX This is so bogus. Figure out how to pull the junk in
			// EmbedCreate() up here or at least rework it so it's cleaner.
			CPluginView* newPlugin = (CPluginView*) app->fe_data;
			newPlugin->EmbedCreate(*mContext, inEmbedStruct);
		}
		Catch_(inErr)
		{
			inEmbedStruct->FE_Data =  NULL;
		}
		EndCatch_
	}
	else
		NPL_EmbedSize((NPEmbeddedApp*) inEmbedStruct->FE_Data);
}

void CHTMLView::FreeEmbedElement(
	LO_EmbedStruct*			inEmbedStruct)
{
	NPL_EmbedDelete(*mContext, inEmbedStruct);
	inEmbedStruct->FE_Data = NULL;
}

void CHTMLView::CreateEmbedWindow(
	NPEmbeddedApp*			inEmbeddedApp)
{
	// Ensure that we have a layout struct so that we can size 
	// the plugin properly.
	ThrowIfNil_(inEmbeddedApp->np_data);
	LO_EmbedStruct* embed_struct = ((np_data*) inEmbeddedApp->np_data)->lo_struct;
	
	ThrowIfNil_(embed_struct);
	
	LCommander::SetDefaultCommander(LWindow::FetchWindowObject(GetMacPort()));
	LPane::SetDefaultView(NULL);
			
	CPluginView* newPlugin = NULL;
	newPlugin = (CPluginView*) UReanimator::ReadObjects('PPob', 4010);
	ThrowIfNil_(newPlugin);
			
	newPlugin->FinishCreate();
	newPlugin->PutInside(this);
	newPlugin->SetSuperCommander(this);

	SDimension16 hyperSize;
	this->GetFrameSize(hyperSize);				// Get the size of the hyperView
	
	newPlugin->EmbedSize(embed_struct, hyperSize);
	
	// XXX Don't we need to create the NPWindow struct here, too?
	// Ugh. Actually that's done in CPluginView::EmbedCreate(), which we
	// call after NPL_EmbedCreate(). That probably needs some massaging...
	inEmbeddedApp->fe_data = (void*) newPlugin;
}

void CHTMLView::SaveEmbedWindow(
	NPEmbeddedApp*			inEmbeddedApp)
{
	ThrowIfNil_(inEmbeddedApp);
	CPluginView *view = (CPluginView*) inEmbeddedApp->fe_data;
	
	ThrowIfNil_(view);

	//	Make sure that we are not targeting the plugin view
	//
	//	XXX Note that this will be overly aggressive in removing the
	//	focus. Probably what we really want to do is check to see if
	//	some sub-pane of the view has focus, and if so, reset it to
	//	a well-known safe place.
	LCommander::SwitchTarget(NULL);

	//	Un-intsall the plugin view, hide it, and re-target
	//	it to the owning window.	
	view->Hide();
	
	// PCB:  clear the plugin's knowledge that it has been positioned, so it will be layed out correctly.
	view->SetPositioned(false);

	LView *previousParentView = NULL;
	LView *currentParentView = view->GetSuperView();
	while (currentParentView != NULL) {
		previousParentView = currentParentView;
		currentParentView = currentParentView->GetSuperView();
	}
	
	view->PutInside((LWindow *)previousParentView);
	view->SetSuperCommander((LWindow *)previousParentView);
	
	//	XXX This should probably move to the Stop() method of the JVM plugin.
	//FlushEventHierarchy(view);
}

void CHTMLView::RestoreEmbedWindow(
	NPEmbeddedApp*			inEmbeddedApp)
{
	CPluginView* view = (CPluginView*) inEmbeddedApp->fe_data;
	LView* parentView = view->GetSuperView();
	
	//	If we are parented inside the outermost window, then 
	//	reparent us to the hyperview.
	
	if (parentView->GetSuperView() == NULL) {
		view->PutInside(this);
		view->SetSuperCommander(this);
		
		int32 xp = 0;
		int32 yp = 0;
		
		if (XP_OK_ASSERT(inEmbeddedApp->np_data)) {
			LO_EmbedStruct* embed_struct = ((np_data*) inEmbeddedApp->np_data)->lo_struct;
			if (XP_OK_ASSERT(embed_struct)) {
				xp = embed_struct->x + embed_struct->x_offset
					/* - CONTEXT_DATA(*mContext)->document_x */;
				yp = embed_struct->y + embed_struct->y_offset
					/* - CONTEXT_DATA(*mContext)->document_y */;
			}
		}
		
		view->PlaceInSuperImageAt(xp, yp, TRUE);
	}

	LCommander::SetDefaultCommander(LWindow::FetchWindowObject(GetMacPort()));
	LPane::SetDefaultView(NULL);
}

void CHTMLView::DestroyEmbedWindow(
	NPEmbeddedApp*			inEmbeddedApp)
{
	if (inEmbeddedApp && inEmbeddedApp->fe_data)
	{
		// XXX Why does EmbedFree need the LO_EmbedStruct?
		ThrowIfNil_(inEmbeddedApp->np_data);
		LO_EmbedStruct* embed_struct = ((np_data*) inEmbeddedApp->np_data)->lo_struct;
	
		// XXX The following check crashes (why?) when embed_struct is NULL, which it always is.
		// ThrowIfNil_(embed_struct);
	
		CPluginView* view = (CPluginView*) inEmbeddedApp->fe_data;
		view->EmbedFree(*mContext, embed_struct);
		delete view;
		inEmbeddedApp->fe_data = NULL;
	}
}

void CHTMLView::DisplayEmbed(
	int 					/* inLocation */,
	LO_EmbedStruct*			inEmbedStruct)
{
	NPEmbeddedApp* app = (NPEmbeddedApp*) inEmbedStruct->FE_Data;
	if (app && app->fe_data)
	{
		if ( !FocusDraw() )
			return;
			
		CPluginView* view = (CPluginView*) app->fe_data;
		view->EmbedDisplay(inEmbedStruct, false);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::GetJavaAppSize(
	LO_JavaAppStruct*		inJavaAppStruct,
	NET_ReloadMethod		inReloadMethod)
{
	LJ_GetJavaAppSize(*mContext, inJavaAppStruct, inReloadMethod);
}

static void PR_CALLBACK
FE_SaveJavaWindow(MWContext * /* context */, LJAppletData* /* ad */, void* window)
{
#if defined (JAVA)
	CJavaView* javaAppletView = (CJavaView *)window;

	//	Make sure that we are not targeting this
	//	java applet.
	
	LCommander::SwitchTarget(GetContainerWindow(javaAppletView));

	//	Un-intsall the java view, hide it, and re-target
	//	it to the owning window.	

	javaAppletView->Hide();

	LView	*currentParentView,
			*previousParentView = NULL;
	
	currentParentView = javaAppletView->GetSuperView();
	while (currentParentView != NULL)
		{
		previousParentView = currentParentView;
		currentParentView = currentParentView->GetSuperView();
		}
	
	javaAppletView->PutInside((LWindow *)previousParentView);
	javaAppletView->SetSuperCommander((LWindow *)previousParentView);
	
	FlushEventHierarchy(javaAppletView);
#endif /* defined (JAVA) */
}

void CHTMLView::HideJavaAppElement(
	LJAppletData*				inAppletData)
{
    LJ_HideJavaAppElement(*mContext, inAppletData, FE_SaveJavaWindow);
}

static void PR_CALLBACK 
FE_DisplayNoJavaIcon(MWContext *, LO_JavaAppStruct *)
{
    /* write me */
}

static void* PR_CALLBACK 
FE_CreateJavaWindow(MWContext *context, LO_JavaAppStruct * /* inJavaAppStruct */,
					int32 xp, int32 yp, int32 xs, int32 ys)
{
#if defined (JAVA)
	CNSContext* theNSContext = ExtractNSContext(context);
	Assert_(theNSContext != NULL);
	CHTMLView* theCurrentView = ExtractHyperView(*theNSContext);
	Assert_(theCurrentView != NULL);

    CJavaView *newJavaView = NULL;
	Try_
	{
		LCommander::SetDefaultCommander(LWindow::FetchWindowObject(theCurrentView->GetMacPort()));
		LPane::SetDefaultView(NULL);
		newJavaView = (CJavaView *)UReanimator::ReadObjects('PPob', 2090);
		newJavaView->FinishCreate();
		newJavaView->PutInside(theCurrentView);
		newJavaView->SetSuperCommander(theCurrentView);
		newJavaView->PlaceInSuperImageAt(xp, yp, TRUE);

		newJavaView->SetPositioned();
	
/*
		// If the plugin size is 1x1, this really means that the plugin is
		// full-screen, and that we should set up the pluginÕs real width
		// for XP since it doesnÕt know how big to make it.  Since a full-
		// screen plugin should resize when its enclosing view (the hyperview)
		// resizes, we bind the plugin view on all sides to its superview.
		// -bing 11/16/95 
		//
		if (inJavaAppStruct->width == 1 && inJavaAppStruct->height == 1) {
			SBooleanRect binding = {true, true, true, true};
			newJavaView->SetFrameBinding(binding);
			SDimension16 hyperSize;
			this->GetFrameSize(hyperSize);				// Get the size of the hyperView
			inJavaAppStruct->width = hyperSize.width - 1;
			inJavaAppStruct->height = hyperSize.height - 1;
			
			const short kLeftMargin = 8;				// ¥¥¥ These should be defined in mhyper.h!!!
			const short kTopMargin = 8;
			inJavaAppStruct->x -= kLeftMargin;				// Allow the plugin to use ALL the screen space
			inJavaAppStruct->y -= kTopMargin;
		}
*/
	}

	Catch_(inErr)
	{
	}
	EndCatch_

	//	Resize the image
	
	if (newJavaView != NULL) {
		newJavaView->ResizeImageTo(xs, ys, FALSE);
		newJavaView->ResizeFrameTo(xs, ys, FALSE);
	}

	return (void*)newJavaView;
#else
	return (void*)NULL;
#endif /* defined (JAVA) */
}

static void* PR_CALLBACK
FE_GetAwtWindow(MWContext * /* context */, LJAppletData* ad)
{
#if defined (JAVA)
	CJavaView* newJavaView = (CJavaView *)ad->window;
	return (void*)newJavaView->GetUserCon();
#else
	return (void*)NULL;
#endif /* defined (JAVA) */
}

static void PR_CALLBACK 
FE_RestoreJavaWindow(MWContext *context, LJAppletData* ad,
				  	 int32 xp, int32 yp, int32 /* xs */, int32 /* ys */)
{
#if defined (JAVA)
	CNSContext* theNSContext = ExtractNSContext(context);
	Assert_(theNSContext != NULL);
	CHTMLView* theCurrentView = ExtractHyperView(*theNSContext);
	Assert_(theCurrentView != NULL);

	CJavaView* newJavaView = (CJavaView *)ad->window;
		
	LView		*parentView = newJavaView->GetSuperView();
	
	//	If we are parented inside the outermost window, then 
	//	reparent us to the hyperview.
	
	if (parentView->GetSuperView() == NULL) {
		
		newJavaView->PutInside(theCurrentView);
		newJavaView->SetSuperCommander(theCurrentView);
		newJavaView->PlaceInSuperImageAt(xp, yp, TRUE);
			
	}

	LCommander::SetDefaultCommander(LWindow::FetchWindowObject(theCurrentView->GetMacPort()));
	LPane::SetDefaultView(NULL);
#endif /* defined (JAVA) */
}

static void PR_CALLBACK 
FE_SetJavaWindowPos(MWContext * /* context */, void* window,
					int32 xp, int32 yp, int32 /* xs */, int32 /* ys */)
{
#if defined (JAVA)
	CJavaView* newJavaView = (CJavaView *)window;
		
	newJavaView->PlaceInSuperImageAt(xp, yp, TRUE);
#endif /* defined (JAVA) */
}

static void PR_CALLBACK 
FE_SetJavaWindowVisibility(MWContext *context, void* window, PRBool visible)
{
#if defined (JAVA)
	CNSContext* theNSContext = ExtractNSContext(context);
	Assert_(theNSContext != NULL);
	CHTMLView* theCurrentView = ExtractHyperView(*theNSContext);
	Assert_(theCurrentView != NULL);

	CJavaView* newJavaView = (CJavaView *)window;
    Boolean				isComponentVisible = TRUE;
	Hsun_awt_macos_MComponentPeer 	*componentPeer = PaneToPeer(newJavaView); 

	if (componentPeer != NULL) {
		if (unhand(unhand(componentPeer)->target)->visible)
			isComponentVisible = TRUE;
		else
			isComponentVisible = FALSE;
	}

	if (newJavaView != NULL) {
		// This call could mean that either the visibility or the position of the
		// applet has changed, so we make the appropriate changes to the view.
		if ((newJavaView->IsVisible() && !visible) ||
			(isComponentVisible == FALSE))
			newJavaView->Hide();
			
		if ((!newJavaView->IsVisible() && visible) &&
			isComponentVisible)
			newJavaView->Show();
	}
#endif /* defined (JAVA) */
}

void CHTMLView::DisplayJavaApp(
	int 					/* inLocation */,
	LO_JavaAppStruct*		inJavaAppStruct)
{
    LJ_DisplayJavaApp(*mContext, inJavaAppStruct,
					  FE_DisplayNoJavaIcon,
					  FE_GetFullWindowSize,
					  FE_CreateJavaWindow,
					  FE_GetAwtWindow,
					  FE_RestoreJavaWindow,
					  FE_SetJavaWindowPos,
					  FE_SetJavaWindowVisibility);
}

static void PR_CALLBACK
FE_FreeJavaWindow(MWContext * /* context */, struct LJAppletData * /* appletData */,
		  void* window)
{
#if defined (JAVA)
	CJavaView* javaAppletView = (CJavaView *)window;
	delete javaAppletView;
#endif /* defined (JAVA) */
}

void CHTMLView::FreeJavaAppElement(
	LJAppletData*			inAppletData)
{
	LJ_FreeJavaAppElement(*mContext, inAppletData, 
						  FE_SaveJavaWindow,
						  FE_FreeJavaWindow);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::DrawJavaApp(
	int 					/*inLocation*/,
	LO_JavaAppStruct*		)
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::HandleClippingView(
	struct LJAppletData*	, 
	int 					/*x*/, 
	int 					/*y*/, 
	int 					/*width*/, 
	int 					/*height*/)
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::GetFormElementInfo(
	LO_FormElementStruct* 	inElement)
{
	UFormElementFactory::MakeFormElem(this, mContext, inElement);
}


void CHTMLView::ResetFormElementData(
	LO_FormElementStruct*	inElement,
	Boolean 				inRefresh,
	Boolean					inFromDefaults)
{
	UFormElementFactory::ResetFormElementData(inElement, inRefresh, inFromDefaults, true);
}




void CHTMLView::DisplayFormElement(
	int 					/* inLocation */,
	LO_FormElementStruct* 	inFormElement)
{
	CDrawable *currentDrawable = mCurrentDrawable;
	
	// When we're drawing form elements, we force our current drawable to be onscreen
	SetCurrentDrawable(nil);
	UFormElementFactory::DisplayFormElement(mContext, inFormElement);
	SetCurrentDrawable(currentDrawable);
}

void CHTMLView::DisplayBorder(
	int 					/* inLocation */,
	int 					inX,
	int						inY,
	int						inWidth,
	int						inHeight,
	int						inBW,
	LO_Color*	 			inColor,
	LO_LineStyle			inStyle)
{
	if (!FocusDraw() || (inBW == 0))
		return;

	SPoint32 topLeftImage;
	Point topLeft;
	int32 layerOriginX, layerOriginY;
	Rect	borderRect;
	RGBColor borderColor;
	
	if ( mCurrentDrawable != NULL ) {
		mCurrentDrawable->GetLayerOrigin(&layerOriginX, &layerOriginY);
	}
	else {
		layerOriginX = mLayerOrigin.h;
		layerOriginY = mLayerOrigin.v;
	}
	topLeftImage.h = inX + layerOriginX;
	topLeftImage.v = inY + layerOriginY;
	ImageToLocalPoint(topLeftImage, topLeft);

	borderRect.left = topLeft.h;
	borderRect.top = topLeft.v;	
	borderRect.right = borderRect.left + inWidth;
	borderRect.bottom = borderRect.top + inHeight;

	borderColor = UGraphics::MakeRGBColor(inColor->red, inColor->green, inColor->blue);
	RGBForeColor(&borderColor);
	::PenSize(inBW, inBW);

	switch (inStyle) {
		case LO_SOLID:
		::FrameRect(&borderRect);
		break;
		
		case LO_BEVEL:
		::UGraphics::FrameRectShaded(borderRect, FALSE);
		break;
		
		default:
		break;
	}

	::PenSize(1, 1);
}

void CHTMLView::UpdateEnableStates()
{
	// this is a Composer function so that the state of buttons can change(enabled/disabled)
}

void CHTMLView::DisplayFeedback(
					int 		/*inLocation*/,
					LO_Element*	)
{
	// this is a Composer function for showing selection of Images
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::FreeEdgeElement(
	LO_EdgeStruct*			inEdgeStruct)
{
	if (FocusDraw())
		{
		Rect theEdgeFrame;
		if (CalcElementPosition((LO_Element*)inEdgeStruct, theEdgeFrame))
			{
			::InsetRect(&theEdgeFrame, -1, -1);
			::InvalRect(&theEdgeFrame);
			}
		}
	
	// 97-06-11 pkc -- delete inEdgeStruct from edge list
	// NOTE: This line assumes that inEdgeStruct is in the list.
	vector<LO_EdgeStruct*>::iterator found = find(mGridEdgeList.begin(), mGridEdgeList.end(), inEdgeStruct);
	if (found != mGridEdgeList.end())
		mGridEdgeList.erase(found);
}

void CHTMLView::DisplayEdge(
	int 					/* inLocation */,
	LO_EdgeStruct*			inEdgeStruct)
{
	if ( !FocusDraw() )
		return;

	// 97-06-21 pkc -- In some instances, like when the Security Advisor brings the Page Info
	// window to the front, FocusDraw won't call SetPort on the window port for some reason.
	// Use an StPortOriginState to make sure the port is set correctly.
	StPortOriginState theOriginSaver((GrafPtr)GetMacPort());

	if (IsRootHTMLView())
		mShowFocus = FALSE;
	
	// 97-06-11 pkc -- If we're not redrawing a grid edge via DrawSelf, and we're also not
	// drawing because the user dragged an edge, then add this LO_EdgeStruct* to list
	if (!mDontAddGridEdgeToList)
	{
		mGridEdgeList.push_back(inEdgeStruct);
	}
	
	Rect		docFrame; 
	Boolean		isVisible;
	isVisible = CalcElementPosition( (LO_Element*)inEdgeStruct, docFrame );
	int32		size;
	
	if ( !isVisible )
		return;

	SBooleanRect thePartialBevels = { true, true, true, true };
	if ( inEdgeStruct->is_vertical )
		{
		size = inEdgeStruct->width;
		/* top - 2 ?  Try anything else and look closely at the top of the frame edge,
		   where it abutts the enclosing bevel view.  -2 seems to be only benevolent;
		   if you find otherwise, more tweaking may be necessary.  I suspect the unexpected
		   extra pixel correction has something to do with the unfortunate circumstance
		   that the first time an edge is drawn, while the frame is being laid out,
		   the port origin is often one pixel off (both vertically and horizontally)
		   from its final position after layout is complete. */
		docFrame.top -= 2;
		docFrame.bottom++;
		thePartialBevels.top = thePartialBevels.bottom = false;
		}
	else
		{
		size = inEdgeStruct->height;
		::InsetRect( &docFrame, -1, 0 );
		thePartialBevels.left = thePartialBevels.right = false;
		}

	StClipRgnState theClipSaver(docFrame);

	SBevelColorDesc theBevelColors;		
	if ( inEdgeStruct->bg_color )
		{
		UGraphics::SetIfColor( UGraphics::MakeRGBColor( inEdgeStruct->bg_color->red,
			inEdgeStruct->bg_color->green,
			inEdgeStruct->bg_color->blue ) );
		::FillRect( &docFrame, &qd.black );
		}
    else
		{
		// Cinco de Mayo '97 pkc
		// Added code to draw using background pattern like our pattern bevel views
		if (!mPatternWorld)
			{
			mPatternWorld = CSharedPatternWorld::CreateSharedPatternWorld(10000);
			if (mPatternWorld)
				{
				mPatternWorld->AddUser(this);
				}
			}

		if (mPatternWorld)
			{
			Point theAlignment;
			CSharedPatternWorld::CalcRelativePoint(this, CSharedPatternWorld::eOrientation_Port, theAlignment);
			
			CGrafPtr thePort = (CGrafPtr)GetMacPort();

			mPatternWorld->Fill(thePort, docFrame, theAlignment);
			}
		else
			{
			UGraphicGizmos::LoadBevelTraits(10000, theBevelColors);
			::PmForeColor(theBevelColors.fillColor);
			::PaintRect(&docFrame);
			}
		}

	if ( size > 2 )
		{
		// shrink the beveled edges rect back to the original size specified
		if ( inEdgeStruct->is_vertical )
			::InsetRect( &docFrame, 0, 1 );
		else
			::InsetRect( &docFrame, 1, 0 );
		if ( inEdgeStruct->bg_color )
			{
			UGraphics::FrameRectShaded( docFrame, FALSE );
			}
		else
			{
			// Cinco de Mayo '97 pkc
			// Use BevelTintPartialRect if we're drawing using a pattern
			if (mPatternWorld)
				{
				UGraphicGizmos::BevelTintPartialRect(docFrame, 1, 
							0x2000, 0x2000, thePartialBevels);
				}
			else
				{
				UGraphicGizmos::BevelPartialRect(docFrame, 1, 
							theBevelColors.topBevelColor, theBevelColors.bottomBevelColor, thePartialBevels);
				}
			}

		if ( inEdgeStruct->movable )
			{
			Rect theDotFrame = { 0, 0, 2, 2 };
			UGraphicGizmos::CenterRectOnRect(theDotFrame, docFrame);
			UGraphics::FrameCircleShaded( theDotFrame, TRUE );
			}
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::DisplayTable(
	int 					/* inLocation */,
	LO_TableStruct*			inTableStruct)
{
	if (!FocusDraw() ||
		((inTableStruct->border_width == 0) &&
		 (inTableStruct->border_top_width == 0) &&
		 (inTableStruct->border_right_width == 0) &&
		 (inTableStruct->border_bottom_width == 0) &&
		 (inTableStruct->border_left_width == 0)))
		return;

	// Retain akbar (3.0) functionality -- we used to call UGraphics::FrameRectShaded
	// which set a light rgb pin color of { 60000, 60000, 60000 }. By default,
	// UGraphicGizmos has a light pin color of { 0xFFFF, 0xFFFF, 0xFFFF }. So as
	// not to diverge from akbar, set UGraphicGizmos::sLighter then restore its value
	RGBColor savedLighter = UGraphicGizmos::sLighter;
	UGraphicGizmos::sLighter.red = 0xFFFF;
	UGraphicGizmos::sLighter.green = 0xFFFF;
	UGraphicGizmos::sLighter.blue = 0xFFFF;
	// ¥Êelement->width + borderWidth, element->height + borderWidth);
	Rect theFrame;
	if (CalcElementPosition( (LO_Element*)inTableStruct, theFrame ))
		{
		RGBColor borderColor =
			UGraphics::MakeRGBColor((inTableStruct->border_color).red,
									(inTableStruct->border_color).green,
									(inTableStruct->border_color).blue);
		switch (inTableStruct->border_style)
			{
			case BORDER_NONE:
				break;
			
			case BORDER_DOTTED:
			case BORDER_DASHED:
			case BORDER_SOLID:
				DisplaySolidBorder(theFrame,
								   borderColor,
								   inTableStruct->border_top_width,
								   inTableStruct->border_left_width,
								   inTableStruct->border_bottom_width,
								   inTableStruct->border_right_width);
				break;
			
			case BORDER_DOUBLE:
				{
				Int32 borderTopWidth = inTableStruct->border_top_width / 3;
				Int32 borderLeftWidth = inTableStruct->border_left_width / 3;
				Int32 borderBottomWidth = inTableStruct->border_bottom_width / 3;
				Int32 borderRightWidth = inTableStruct->border_right_width / 3;

				// draw outer border
				DisplaySolidBorder(theFrame,
								   borderColor,
								   borderTopWidth,
								   borderLeftWidth,
								   borderBottomWidth,
								   borderRightWidth);
				// adjust frame
				theFrame.top += (inTableStruct->border_top_width - borderTopWidth);
				theFrame.left += (inTableStruct->border_left_width - borderLeftWidth);
				theFrame.bottom -= (inTableStruct->border_bottom_width - borderBottomWidth);
				theFrame.right -= (inTableStruct->border_right_width - borderRightWidth);

				// draw inner border
				DisplaySolidBorder(theFrame,
								   borderColor,
								   borderTopWidth,
								   borderLeftWidth,
								   borderBottomWidth,
								   borderRightWidth);
				}
				break;
			
			case BORDER_GROOVE:
				// Groove border has sunken outer border with a raised inner border
				DisplayGrooveRidgeBorder(theFrame,
										 borderColor,
										 true,
										 inTableStruct->border_top_width,
										 inTableStruct->border_left_width,
										 inTableStruct->border_bottom_width,
										 inTableStruct->border_right_width);
				break;
			case BORDER_RIDGE:
				// Ridge border has raised outer border with a sunken inner border
				DisplayGrooveRidgeBorder(theFrame,
										 borderColor,
										 false,
										 inTableStruct->border_top_width,
										 inTableStruct->border_left_width,
										 inTableStruct->border_bottom_width,
										 inTableStruct->border_right_width);
				break;
			
			case BORDER_INSET:
				// sunken border
				DisplayBevelBorder(theFrame,
								   borderColor,
								   false,
								   inTableStruct->border_top_width,
								   inTableStruct->border_left_width,
								   inTableStruct->border_bottom_width,
								   inTableStruct->border_right_width);
				break;

			case BORDER_OUTSET:
				// raised border
				DisplayBevelBorder(theFrame,
								   borderColor,
								   true,
								   inTableStruct->border_top_width,
								   inTableStruct->border_left_width,
								   inTableStruct->border_bottom_width,
								   inTableStruct->border_right_width);
				break;
			
			default:
				Assert_(false);
			}
					
		// restore UGraphicGizmos::sLighter
		UGraphicGizmos::sLighter = savedLighter;
		}
}

void CHTMLView::DisplaySolidBorder(
	const Rect&				inFrame,
	const RGBColor&			inBorderColor,
	Int32					inTopWidth,
	Int32					inLeftWidth,
	Int32					inBottomWidth,
	Int32					inRightWidth)
{
	StColorPenState state;
	state.Normalize();
	::RGBForeColor(&inBorderColor);
	// Check for easy case -- all border widths equal
	if (inTopWidth == inLeftWidth &&
		inTopWidth == inBottomWidth &&
		inTopWidth == inRightWidth)
	{
		::PenSize(inTopWidth, inTopWidth);
		::FrameRect(&inFrame);
	}
	else
	{
		// Otherwise manually draw each side
		if (inTopWidth > 0)
		{
			::PenSize(1, inTopWidth);
			::MoveTo(inFrame.left, inFrame.top);
			::LineTo(inFrame.right, inFrame.top);
		}
		if (inLeftWidth > 0)
		{
			::PenSize(inLeftWidth, 1);
			::MoveTo(inFrame.left, inFrame.top);
			::LineTo(inFrame.left, inFrame.bottom);
		}
		if (inBottomWidth > 0)
		{
			::PenSize(1, inBottomWidth);
			// Don't forget, pen draws down and to the right
			::MoveTo(inFrame.left, inFrame.bottom - inBottomWidth);
			::LineTo(inFrame.right, inFrame.bottom - inBottomWidth);
		}
		if (inRightWidth > 0)
		{
			::PenSize(inRightWidth, 1);
			// Don't forget, pen draws down and to the right
			::MoveTo(inFrame.right - inRightWidth, inFrame.bottom);
			::LineTo(inFrame.right - inRightWidth, inFrame.top);
		}
	}
}

Uint16 AddWithoutOverflow(Uint16 base, Uint16 addition)
{
	if ((base + addition) > 0xFFFF)
	{
		// overflow, return max Uint16 value
		return 0xFFFF;
	}
	else
		return base + addition;
}

Uint16 SubWithoutUnderflow(Uint16 base, Uint16 difference)
{
	if ((base - difference) < 0x0000)
	{
		// underflow, return 0
		return 0x0000;
	}
	else
		return base - difference;
}

void ComputeBevelColor(Boolean inRaised, RGBColor inBaseColor, RGBColor& outBevelColor)
{
	if (inRaised)
	{
		outBevelColor.red = AddWithoutOverflow(inBaseColor.red, TableBorder_TintLevel);
		outBevelColor.green = AddWithoutOverflow(inBaseColor.green, TableBorder_TintLevel);
		outBevelColor.blue = AddWithoutOverflow(inBaseColor.blue, TableBorder_TintLevel);
	}
	else
	{
		outBevelColor.red = SubWithoutUnderflow(inBaseColor.red, TableBorder_TintLevel);
		outBevelColor.green = SubWithoutUnderflow(inBaseColor.green, TableBorder_TintLevel);
		outBevelColor.blue = SubWithoutUnderflow(inBaseColor.blue, TableBorder_TintLevel);
	}
}

void CHTMLView::DisplayBevelBorder(
	const Rect&				inFrame,
	const RGBColor&			inBorderColor,
	Boolean					inRaised,
	Int32					inTopWidth,
	Int32					inLeftWidth,
	Int32					inBottomWidth,
	Int32					inRightWidth)
{
	// 97-06-10 pkc -- No longer use UGraphicGizmos::BevelRect
	StColorPenState state;
	state.Normalize();
	PolyHandle poly = NULL;
	RGBColor raisedBevelColor, loweredBevelColor;
	ComputeBevelColor(true, inBorderColor, raisedBevelColor);
	ComputeBevelColor(false, inBorderColor, loweredBevelColor);
	// First do top and left sides
	if (inTopWidth > 0 || inLeftWidth > 0)
	{
		poly = OpenPoly();
		if (inRaised)
			::RGBForeColor(&raisedBevelColor);
		else
			::RGBForeColor(&loweredBevelColor);
		::MoveTo(inFrame.left, inFrame.top);
		if (inTopWidth > 0)
		{
			::LineTo(inFrame.right, inFrame.top);
			::LineTo(inFrame.right - inRightWidth, inFrame.top + inTopWidth);
			::LineTo(inFrame.left + inLeftWidth, inFrame.top + inTopWidth);
		}
		if (inLeftWidth > 0)
		{
			if (inTopWidth == 0)
				::LineTo(inFrame.left + inLeftWidth, inFrame.top + inTopWidth);
			::LineTo(inFrame.left + inLeftWidth, inFrame.bottom - inBottomWidth);
			::LineTo(inFrame.left, inFrame.bottom);
		}
		::ClosePoly();
		::PaintPoly(poly);
		::KillPoly(poly);
		poly = NULL;
	}
	if (inRightWidth > 0 || inBottomWidth > 0)
	{
		poly = OpenPoly();
		// Then do bottom and right sides
		if (inRaised)
			::RGBForeColor(&loweredBevelColor);
		else
			::RGBForeColor(&raisedBevelColor);
		::MoveTo(inFrame.right, inFrame.bottom);
		if (inRightWidth > 0)
		{
			::LineTo(inFrame.right, inFrame.top);
			::LineTo(inFrame.right - inRightWidth, inFrame.top + inTopWidth);
			::LineTo(inFrame.right - inRightWidth, inFrame.bottom - inBottomWidth);
		}
		if (inBottomWidth > 0)
		{
			if (inRightWidth == 0)
				::LineTo(inFrame.right - inRightWidth, inFrame.bottom - inBottomWidth);
			::LineTo(inFrame.left + inLeftWidth, inFrame.bottom - inBottomWidth);
			::LineTo(inFrame.left, inFrame.bottom);
		}
		::ClosePoly();
		::PaintPoly(poly);
		::KillPoly(poly);
		poly = NULL;
	}
}

void CHTMLView::DisplayGrooveRidgeBorder(
	const Rect&				inFrame,
	const RGBColor&			inBorderColor,
	Boolean					inIsGroove,
	Int32					inTopWidth,
	Int32					inLeftWidth,
	Int32					inBottomWidth,
	Int32					inRightWidth)
{
	Rect theFrame = inFrame;
	Int32 borderTopWidth = inTopWidth / 2;
	Int32 borderLeftWidth = inLeftWidth / 2;
	Int32 borderBottomWidth = inBottomWidth / 2;
	Int32 borderRightWidth = inRightWidth / 2;

	// draw outer border
	DisplayBevelBorder(theFrame,
					   inBorderColor,
					   inIsGroove ? false : true,
					   borderTopWidth,
					   borderLeftWidth,
					   borderBottomWidth,
					   borderRightWidth);
	// adjust frame
	theFrame.top += borderTopWidth;
	theFrame.left += borderLeftWidth;
	theFrame.bottom -= borderBottomWidth;
	theFrame.right -= borderRightWidth;
	// draw inner border
	DisplayBevelBorder(theFrame,
					   inBorderColor,
					   inIsGroove ? true : false,
					   borderTopWidth,
					   borderLeftWidth,
					   borderBottomWidth,
					   borderRightWidth);
}

void CHTMLView::DisplayCell(
	int 					/* inLocation */,
	LO_CellStruct*			inCellStruct)
{
	if (!FocusDraw() )
		return;

	// ¥ subdoc->width + borderWidth, subdoc->height + borderWidth);
	Rect theFrame;
	if (CalcElementPosition( (LO_Element*)inCellStruct, theFrame ))
		{

		// ¥Êthis is really slow LAM
		for ( int i = 1; i <= inCellStruct->border_width; i++ )
			{
			UGraphics::FrameRectShaded( theFrame, true );
			::InsetRect( &theFrame, 1, 1 );
			}
		}
}

void CHTMLView::InvalidateEntireTableOrCell(LO_Element*)
{
	/* composer only */
}

void CHTMLView::DisplayAddRowOrColBorder(XP_Rect*, XP_Bool	/*inDoErase*/)
{
	/* composer only */
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CHTMLView::CalcElementPosition(
	LO_Element* 			inElement,
	Rect& 					outFrame)
{
	//	This function takes the real position of a layout element,
	//	.. and returns the local position and whether it is visible.
	//	the calculated position is valid, even if the element is not currently visible

	XP_Rect			absoluteFrame;
	Point			portOrigin;
	SPoint32		frameLocation;
	SDimension16	frameSize;
	SPoint32		imageLocation;
	Boolean			isVisible;

	portOrigin.h = 0; portOrigin.v = 0;
	PortToLocalPoint (portOrigin);
	GetFrameLocation (frameLocation);
	GetFrameSize (frameSize);
	GetImageLocation (imageLocation);

	long realFrameLeft 		= frameLocation.h - imageLocation.h;
	long realFrameRight 	= realFrameLeft + frameSize.width;
	long realFrameTop 		= frameLocation.v - imageLocation.v;
	long realFrameBottom 	= realFrameTop + frameSize.height;

	CalcAbsoluteElementPosition (inElement, absoluteFrame);

	isVisible = realFrameRight > absoluteFrame.left
		&&	realFrameLeft < absoluteFrame.right
		&&	realFrameBottom > absoluteFrame.top
		&&	realFrameTop < absoluteFrame.bottom;

	outFrame.left = absoluteFrame.left + portOrigin.h + imageLocation.h;
	outFrame.top = absoluteFrame.top + portOrigin.v + imageLocation.v;
	outFrame.right = outFrame.left + inElement->lo_any.width;
	outFrame.bottom = outFrame.top + inElement->lo_any.height;

	return isVisible;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::CalcAbsoluteElementPosition(
	LO_Element*				inElement,
	XP_Rect&				outFrame)
{
	// calculates the position of a layout element within the document, disregarding
	// scroll positions and whatnot

	// make sure we have the actual element position
	long elementPosLeft 	= inElement->lo_any.x + inElement->lo_any.x_offset;
	long elementPosTop 		= inElement->lo_any.y + inElement->lo_any.y_offset;

#ifdef LAYERS
	int32	layerOriginX,layerOriginY;

	if (mCurrentDrawable != NULL)
		mCurrentDrawable->GetLayerOrigin ( &layerOriginX, &layerOriginY );
	else {
		layerOriginX = mLayerOrigin.h;
		layerOriginY = mLayerOrigin.v;
	}
		
	elementPosLeft += layerOriginX;
	elementPosTop += layerOriginY;
#endif

	outFrame.left = elementPosLeft;
	outFrame.top = elementPosTop;
	outFrame.right = outFrame.left + inElement->lo_any.width;
	outFrame.bottom = outFrame.top + inElement->lo_any.height;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CHTMLView::CreateGridView(
	CBrowserContext*		inGridContext,
	Int32					inX,
	Int32 					inY,
	Int32					inWidth,
	Int32					inHeight,
	Int8					inScrollMode,
	Bool 					inNoEdge)
{

	CHyperScroller* theContainer = NULL;

	try
	{
//		URL_Struct*			request = NULL;
//		CHyperScroller*		theContainer;
//		MWContext*			newContext;
//		CHyperView*			newView;

		SetScrollMode(LO_SCROLL_NO);
		mShowFocus = false;
		
		FocusDraw();
		LCommander::SetDefaultCommander( this );
		LPane::SetDefaultView( this );

		theContainer = (CHyperScroller*)UReanimator::ReadObjects('PPob', 1005);
		ThrowIfNULL_(theContainer);
		theContainer->FinishCreate();
		
		CHTMLView* theNewView = (CHTMLView*)theContainer->FindPaneByID(2005);
		ThrowIfNULL_(theNewView);
	
		// ¥ position it
		theNewView->mNoBorder = inNoEdge;
		theNewView->SetSuperHTMLView(this);

		Rect theOwningFrame;
		CalcLocalFrameRect(theOwningFrame);

		Rect theNewFrame;
		CropFrameToContainer(inX, inY, inWidth, inHeight, theNewFrame);

		CHyperScroller* theSuperScroller = mScroller;
		if (!theNewView->mNoBorder)
			{
			//	for each side that intersects the container, 
			//	expand the container out one
			if (inX == 0)
				if (theSuperScroller)
					theSuperScroller->ExpandLeft();
				
			if (inY == 0)
				if (theSuperScroller)
					theSuperScroller->ExpandTop();

			if (theNewFrame.right == theOwningFrame.right)
				if (theSuperScroller)
					theSuperScroller->ExpandRight();
				
			if (theNewFrame.bottom == theOwningFrame.bottom)
				if (theSuperScroller)
					theSuperScroller->ExpandBottom();
			
			if (theSuperScroller)
				theSuperScroller->AdjustHyperViewBounds();
			}
		else
			{
			theContainer->ExpandLeft();
			theContainer->ExpandTop();
			theContainer->ExpandRight();
			theContainer->ExpandBottom();
			theContainer->AdjustHyperViewBounds();
			}
									
		CropFrameToContainer(inX, inY, inWidth, inHeight, theNewFrame);

		theContainer->PlaceInSuperFrameAt(theNewFrame.left, theNewFrame.top, true);
		theContainer->ResizeFrameTo(RectWidth(theNewFrame), RectHeight(theNewFrame), false);

		if (theSuperScroller)
			theSuperScroller->AdjustHyperViewBounds();
		theContainer->AdjustHyperViewBounds();

		theNewView->SetScrollMode(inScrollMode);
		// 97-05-07 pkc -- if inNoEdge is true, we've got a borderless frame, don't
		// display frame focus
		if (!inNoEdge)
			theNewView->mShowFocus = true;

		// ¥ so that we call scroller's activate, and it actually enables the scrollbars
//		theContainer->ActivateSelf();
// FIX ME!!! this used to be the line above.  I hope this still works
		theContainer->Activate();
		
		if (theSuperScroller)
			theSuperScroller->Refresh();

		theNewView->SetContext(inGridContext);
		mHasGridCells = true;
		
			// give the grid context the same listeners as the root context
		mContext->CopyListenersToContext(inGridContext);	
		}
	catch (...)
		{
		delete theContainer;
		throw;
		}

}

void CHTMLView::CropFrameToContainer(
	Int32		inImageLeft,
	Int32		inImageTop,
	Int32		inImageWidth,
	Int32		inImageHeight,
	Rect&		outLocalFrame) const
{
	Rect theOwningFrame;
	CalcLocalFrameRect(theOwningFrame);

	SPoint32 theImageTopLeft;
	theImageTopLeft.h = inImageLeft;
	theImageTopLeft.v = inImageTop;

	SPoint32 theImageBotRight;
	theImageBotRight.h = inImageLeft + inImageWidth;
	theImageBotRight.v = inImageTop + inImageHeight;

	ImageToLocalPoint(theImageTopLeft, topLeft(outLocalFrame));
	ImageToLocalPoint(theImageBotRight, botRight(outLocalFrame));
	
	::SectRect(&theOwningFrame, &outLocalFrame, &outLocalFrame);
}

// ---------------------------------------------------------------------------
//		¥ CalcStandardSizeForWindowForScreen
// ---------------------------------------------------------------------------

void
CHTMLView::CalcStandardSizeForWindowForScreen(
	CHTMLView*			inTopMostHTMLView,
	const LWindow&		inWindow,
	const Rect&			inScreenBounds,
	SDimension16&		outStandardSize)
{
	bool				haveAnHTMLView = false;
	Rect				minMaxSize;
	SDimension16		htmlFrameSize;
	SDimension16		windowContentSize;
	SDimension32		htmlImageSize;
 	CBrowserContext*	theTopContext = nil;
	Boolean				puntToADefaultStandardSize = false;
	
	if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
	{
		outStandardSize.width = max_Int16;
		outStandardSize.height = max_Int16;

		return;
	}
	
	inWindow.GetMinMaxSize(minMaxSize);
	inWindow.GetFrameSize(windowContentSize);
	
	//	Note: If inTopMostHTMLView is nil, then we use the punt size

	if (inTopMostHTMLView)
	{
		theTopContext = inTopMostHTMLView->GetContext();
		
		// Get the image size of the html view
		
		inTopMostHTMLView->GetImageSize(htmlImageSize);
		htmlImageSize.width = MIN(htmlImageSize.width, max_Int16);
		htmlImageSize.height = MIN(htmlImageSize.height, max_Int16);

		if (htmlImageSize.width != 0 && htmlImageSize.height != 0)
		{
			haveAnHTMLView = true;
		}
	}
		
	// Calculate standard size
	
	if (theTopContext && haveAnHTMLView)
	{		
		CBrowserContext* theTopContext = inTopMostHTMLView->GetContext();
		
		ThrowIfNil_(theTopContext);
		
		if (!theTopContext->CountGridChildren())
		{
			// Get the frame size of the html view
			
			inTopMostHTMLView->GetFrameSize(htmlFrameSize);
			
			outStandardSize.width = ((windowContentSize.width - htmlFrameSize.width) + htmlImageSize.width);
			outStandardSize.height = ((windowContentSize.height - htmlFrameSize.height) + htmlImageSize.height);
			
			// Shrink the standard size in each dimension if the other dimension has a scroll bar
			// which will disappear at the new standard size. We attempt to adjust the standard
			// size height first. If the standard width for the screen does cause a previous
			// horizontal scrollbar to disappear then we adjust the height. We remember whether
			// it was adjusted. Then we attempt to adjust the width. Then we try once more on the
			// height if it wasn't already adjusted.
			
			Boolean heightAdjusted = false;
			
			if (inTopMostHTMLView->GetScroller() &&
				inTopMostHTMLView->GetScroller()->HasHorizontalScrollbar() &&
				outStandardSize.width < (inScreenBounds.right - inScreenBounds.left))
			{
				outStandardSize.height -= (ScrollBar_Size - 1);
				heightAdjusted = true;
			}				
			
			if (inTopMostHTMLView->GetScroller() &&
				inTopMostHTMLView->GetScroller()->HasVerticalScrollbar() &&
				outStandardSize.height < (inScreenBounds.bottom - inScreenBounds.top))
			{
				outStandardSize.width -= (ScrollBar_Size - 1);
			}				
				
			if (inTopMostHTMLView->GetScroller() &&
				inTopMostHTMLView->GetScroller()->HasHorizontalScrollbar() &&
				outStandardSize.width < (inScreenBounds.right - inScreenBounds.left) &&
				!heightAdjusted)
			{
				outStandardSize.height -= (ScrollBar_Size - 1);
				heightAdjusted = true;
			}				
	
			// Don't shrink window smaller than the minimum size
			
			if (outStandardSize.width < minMaxSize.left)
				outStandardSize.width = minMaxSize.left;
			
			if (outStandardSize.height < minMaxSize.top)
				outStandardSize.height = minMaxSize.top;
		}
		else
		{
			// We have frames.
			//
			// We will pick a "reasonable" size out of our... Note that Akbar would
			// simply reuse the mStandardSize for the previously viewed page (which
			// produces inconsistent results when zooming a given frames page). At least
			// this method will produce consistent results.
			//
			// It would be cool, of course, if we could figure out which frames should
			// contribute to the calculation of the new width and height of the window
			// to miminize scrolling and do that instead. That would produce a better
			// standard size for frame documents.
			
			puntToADefaultStandardSize = true;
		}
	}
	else
	{
		// No context or top-most context is not and html view or the user held
		// down the optionKey, so we just punt
		
		puntToADefaultStandardSize = true;
	}
	
	if (puntToADefaultStandardSize)
	{
		Int16 height	= 850;
		Int16 width		= 0.85 * height;
		
		// If the punt height is greater than the screen height then
		// recalculate with a punt height equal to the screen height.
		
		if ((inScreenBounds.bottom - inScreenBounds.top) < height)
		{
			height	= inScreenBounds.bottom - inScreenBounds.top;
			width	= 0.85 * height;
		}
		
		outStandardSize.width = width;
		outStandardSize.height = height;
	}
}

void CHTMLView::GetFullGridSize(
	Int32&					outWidth,
	Int32&					outHeight)
{
	// FIX ME!!! here's another thing that will need to change
	// when the scroller dependency is removed.
	if (mScroller != NULL)
		{
		SDimension16 theContainerSize;
		mScroller->GetFrameSize(theContainerSize);
		outWidth = theContainerSize.width;
		outHeight = theContainerSize.height;
		}
	else
		{
		Assert_(false);
		outWidth = outHeight = 0;
		}
}

void CHTMLView::RestructureGridView(
	Int32					inX,
	Int32 					inY,
	Int32					inWidth,
	Int32					inHeight)
{
	mSuperHTMLView->FocusDraw();
	
	if (mScroller != NULL)
		{
		CHyperScroller* theSuperScroller = mScroller;
		
		Rect theNewFrame;
		mSuperHTMLView->CropFrameToContainer(inX, inY, inWidth, inHeight, theNewFrame);
		theSuperScroller->PlaceInSuperFrameAt(theNewFrame.left, theNewFrame.top, false);
		theSuperScroller->ResizeFrameTo(RectWidth(theNewFrame), RectHeight(theNewFrame), false);
		theSuperScroller->AdjustHyperViewBounds();
		}
		
//	if ( mContext )
//	{
//		History_entry* he = SHIST_GetCurrent( &fHContext->hist );
//		if ( !he || he->is_binary )
//		{
//			this->Refresh();
//			this->ReadjustScrollbars( TRUE );
//			this->Refresh();
//			return;
//		}
//	}
	
	// ¥ causes repositioning of the scrollbars
	SetScrollMode(mDefaultScrollMode, true); 
	ClearBackground();
	mContext->Repaginate();
}

/*
// MAY WANT TO ADD HERE AS BSE IMPLEMENTATION
void CHTMLView::BeginPreSection(void)
{
	// Empty NS_xxx implementation
}

void CHTMLView::EndPreSection(void)
{
	// Empty NS_xxx implementation
}

*/

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ CDragURLTask
// ---------------------------------------------------------------------------

CDragURLTask::CDragURLTask(
	const EventRecord&	inEventRecord,
	const Rect& 		inGlobalFrame,
	CHTMLView& 			inHTMLView)
	:	mGlobalFrame(inGlobalFrame),
		mHTMLView(inHTMLView),
	
		super(inEventRecord)
{
}

// ---------------------------------------------------------------------------
//		¥ AddFlavors
// ---------------------------------------------------------------------------
									
void
CDragURLTask::AddFlavors(
	DragReference		/* inDragRef */)
{
	OSErr theErr;

	// If the option-key is down and the element is an image, we drag as a PICT, even to the finder..
	// If not, we drag real image data (JPEF or GIF)..
	Boolean isOptionDown = ((mEventRecord.modifiers & optionKey) != 0);

	if (mHTMLView.mDragElement->type != LO_IMAGE || !isOptionDown)
	{
		AddFlavorBookmarkFile(static_cast<ItemReference>(this));
	}

	AddFlavorURL(static_cast<ItemReference>(this));
	
	AddFlavorBookmark(static_cast<ItemReference>(this));
	
	// Add a PICT flavor for images

	if (mHTMLView.mDragElement->type == LO_IMAGE)
	{
		LO_ImageStruct* imageElement = (LO_ImageStruct*)mHTMLView.mDragElement;

		if (!IsInternalTypeLink((char*)imageElement->image_url) || IsMailNewsReconnect((char*)imageElement->image_url))
		{
			// flavorSenderTranslated will prevent us from being saved as a PICT clipping
			// in the Finder.  When the option key is down, this is exactly what we want
			
			FlavorFlags	flavorFlags = (isOptionDown) ?  0 : flavorSenderTranslated;
			theErr = ::AddDragItemFlavor (mDragRef, (ItemReference) this, 'PICT', nil, 0, flavorFlags);
			ThrowIfOSErr_(theErr);
		}
	}
}			

// ---------------------------------------------------------------------------
//		¥ MakeDragRegion
// ---------------------------------------------------------------------------
	
void
CDragURLTask::MakeDragRegion(
	DragReference			/* inDragRef */,
	RgnHandle				/* inDragRegion */)
{
	AddRectDragItem((ItemReference)mHTMLView.mDragElement, mGlobalFrame);
}
				
#pragma mark -

#if defined (JAVA)
void FlushEventHierarchyRecursive(LPane *currentPane)
{
	Hsun_awt_macos_MComponentPeer	*componentPeerHandle = PaneToPeer(currentPane);

	//	Flush the events associated with the component;

	if (componentPeerHandle != NULL) {
	
		//	Clear the interface queue of events related to the component to help garbage collection.
		
		ClassClass	*interfaceEventClass;
		
		interfaceEventClass = FindClass(EE(), "sun/awt/macos/InterfaceEvent", (PRBool)TRUE);
		
		if (interfaceEventClass != NULL) {
			MToolkitExecutJavaStaticMethod(interfaceEventClass, "flushInterfaceQueue", "(Lsun/awt/macos/MComponentPeer;)V", componentPeerHandle);
		}
	
		//	Recurse on the sub-panes
		
		if (unhand(componentPeerHandle)->mIsContainer) {
		
			LArrayIterator	iterator(((LView *)currentPane)->GetSubPanes());
			LPane			*theSub;
				
			while (iterator.Next(&theSub))
				FlushEventHierarchyRecursive(theSub);
		}

	}
	
}

void FlushEventHierarchy(LView *javaAppletView)
{
	LPane		*canvas = javaAppletView->FindPaneByID('cvpr');

	FlushEventHierarchyRecursive(canvas);

}
#endif /* defined (JAVA) */
