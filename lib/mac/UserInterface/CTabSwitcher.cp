/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include <LArrayIterator.h>
#include <LCommander.h>
#include <LStream.h>
#include <PP_Messages.h>
#include <UMemoryMgr.h>
#include <UReanimator.h>

#include "CTabSwitcher.h"
#include "CTabControl.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CTabSwitcher::CTabSwitcher(LStream* inStream)
	:	LView(inStream),
		mCachedPages(sizeof(LView*))
{
	*inStream >> mTabControlID;
	*inStream >> mContainerID;
	*inStream >> mIsCachingPages;

	mCurrentPage = NULL;
	mSavedValue = 0;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CTabSwitcher::~CTabSwitcher()
{
	FlushCachedPages();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabSwitcher::FinishCreateSelf(void)
{
	LView::FinishCreateSelf();
	
	CTabControl* theTabControl = (CTabControl*)FindPaneByID(mTabControlID);
	Assert_(theTabControl != NULL);
	theTabControl->AddListener(this);

	MessageT theTabMessage = theTabControl->GetValueMessage();
	if (theTabMessage != msg_Nothing)
		{
		SwitchToPage(theTabControl->GetValueMessage());
		mSavedValue = theTabControl->GetValue();
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabSwitcher::ListenToMessage(
	MessageT		inMessage,
	void*			ioParam)
{
	if (inMessage == msg_TabSwitched)
		{
		ResIDT thePageResID = (ResIDT)(*(MessageT*)ioParam);
		SwitchToPage(thePageResID);
		}
	else if (inMessage == msg_GrowZone)
		{
		FlushCachedPages();
		// 1998.01.12 pchen -- replicate fix for bug #85275
		// We don't know how much memory was freed, so just set bytes freed to 0
		*((Int32 *)ioParam) = 0;
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabSwitcher::SwitchToPage(ResIDT inPageResID)
{
	CTabControl* theTabControl = (CTabControl*)FindPaneByID(mTabControlID);
	Assert_(theTabControl != NULL);

	LView* thePageContainer = (LView*)FindPaneByID(mContainerID);
	Assert_(thePageContainer != NULL);
	
	LView* thePage = NULL;
	Boolean	isFromCache = true;
		
	if (mIsCachingPages)
		thePage = FetchPageFromCache(inPageResID);

	if (thePage == NULL)			// we need to reanimate
		{
		try
			{
#if 0
/*
			// We temporarily disable signaling since we're reanimating into a NULL
			// view container.  FocusDraw() signals if it cant establish a port.
			StValueChanger<EDebugAction> okayToSignal(gDebugSignal, debugAction_Nothing);
			thePage = UReanimator::CreateView(inPageResID, NULL, this);
*/
#endif
			thePage = UReanimator::CreateView(inPageResID, this, this);
			isFromCache = false;
			}
		catch (...)
			{
			// something went wrong
			delete thePage;
			thePage = NULL;
			}
		}
	else
		RemovePageFromCache(thePage);
	
	// Sanity check in case we mad a page without setting this up.
	Assert_(thePage->GetPaneID() == inPageResID);
		
	if ((thePage != NULL) && SwitchTarget(this))
		{
		if (mCurrentPage != NULL)
			{
			mCurrentPage->Hide();
			DoPreDispose(mCurrentPage, mIsCachingPages);
			if (mIsCachingPages)
				{
				mCurrentPage->PutInside(NULL, false);
				mCachedPages.InsertItemsAt(1, LArray::index_Last, &mCurrentPage);
				}
			else
				delete mCurrentPage;
			}
		
		thePage->PutInside(thePageContainer);
		thePageContainer->ExpandSubPane(thePage, true, true);

		DoPostLoad(thePage, isFromCache);

		thePage->Show();
		thePage->Refresh();
		mCurrentPage = thePage;
		mSavedValue = theTabControl->GetValue();

//		RestoreTarget();			// walk the latent subs
//		Don't restore the target otherwise you can't
//		switch to another target in DoPostLoad()

		}
	else 							// we falied to reanimate
		{
		theTabControl->SetValue(mSavedValue);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

LView* CTabSwitcher::FindPageByID(ResIDT inPageResID)
{
	LView* theFoundPage = NULL;
	
	if ((mCurrentPage != NULL) && (mCurrentPage->GetPaneID() == inPageResID))
		theFoundPage = mCurrentPage;
	else	
		theFoundPage = FetchPageFromCache(inPageResID);
	
	return theFoundPage;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabSwitcher::DoPostLoad(
	LView* 			/* inLoadedPage */,
	Boolean			/* inFromCache  */)
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabSwitcher::DoPreDispose(
	LView* 			/* inLeavingPage */,
	Boolean			/* inWillCache   */)
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

LView* CTabSwitcher::FetchPageFromCache(ResIDT inPageResID)
{
	LView* theCachedPage = NULL;
	LView* theIndexPage = NULL;
	
	LArrayIterator theIter(mCachedPages, LArrayIterator::from_Start);
	while (theIter.Next(&theIndexPage))
		{
		if (theIndexPage->GetPaneID() == inPageResID)
			{
			theCachedPage = theIndexPage;
			break;
			}
		}

	return theCachedPage;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabSwitcher::RemovePageFromCache(LView* inPage)
{
	Assert_(inPage != NULL);
	
	ArrayIndexT thePageIndex = mCachedPages.FetchIndexOf(&inPage);
	Assert_(thePageIndex != LArray::index_Bad);

	mCachedPages.RemoveItemsAt(1, thePageIndex);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CTabSwitcher::FlushCachedPages(void)
{
	LView* thePage;
	LArrayIterator theIter(mCachedPages, LArrayIterator::from_Start);
	while (theIter.Next(&thePage))
	{
		// We need to put the page back into the view hierarchy otherwise
		// FocusDraw() signals a nil GrafPort and LScroller may crash.
		LView* thePageContainer = (LView*)FindPaneByID(mContainerID);
		Assert_(thePageContainer != NULL);
		thePage->PutInside(thePageContainer);
		// Ok, we can delete the page now.
		delete thePage;
	}

	mCachedPages.RemoveItemsAt(LArray::index_First, mCachedPages.GetCount());
}

