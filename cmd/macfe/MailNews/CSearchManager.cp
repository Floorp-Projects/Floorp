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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CSearchManager.cp

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#define DEBUGGER_ASSERTIONS

#include "CSearchManager.h"
#include "SearchHelpers.h"

#include "uerrmgr.h"
#include "CMailNewsContext.h"
#include "libi18n.h"
#include "uprefd.h"
#include <LGAPopup.h>
#include <LGAPushButton.h>
#include "LGARadioButton.h"
#include "CCaption.h"
#include "prefapi.h"
#include <UModalDialogs.h>
#include "CSingleTextColumn.h"
#include "UMailFilters.h"
#include "CTSMEditField.h"
#include <LCommander.h>
#include "UIHelper.h"
#include "nc_exception.h"

#pragma mark -
/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/

const UInt16 deltaAttributes = 8; // extra array elements to allocate for the attributes menu
const UInt16 resIDT_CustomHeadersDialog = 8652; // resource id for the custom header edit dialog box
const UInt16 resIDT_CustomHeadersInputDialog = 8654; // resource id for the input dialog box for custom header
const UInt16 kMaxStringLength = 256;

const char		kCustomHeaderPref[] = "mailnews.customHeaders";	// preference name for custom headers

// custom headers messages 
const MessageT	msg_NewHeader				= 'chNW';	// new custom header message
const MessageT	msg_EditHeader				= 'chED';	// edit custom header message
const MessageT	msg_DeleteHeader			= 'chDE';	// delete custom header message

/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/
#pragma mark -

//-----------------------------------
CSearchManager::CSearchManager() :
			   LListener(),
			   LBroadcaster(),
			   mNumLevels(0),
			   mNumVisLevels(0),
			   mLastMenuItemsScope((MSG_ScopeAttribute) -1),
			   mLastMenuItemsAttribute((MSG_SearchAttribute) -1),
			   mNumLastMenuItems(0),
			   mCurrentScope((MSG_ScopeAttribute) -1),
			   mMoreButton(nil),
			   mFewerButton(nil),
			   mMsgPane(nil),
			   mCanRotateTarget(true),
			   mBroadcastUserChanges(true),
			   mFolderScopeList(nil),
			   mIsSearching(false),
			   mMatchAllRadio(nil),
			   mMatchAnyRadio(nil),
			   mBoolOperator( true ),
			   mAttributeItems( nil ),
			   mLastNumOfAttributes( 0 )
//-----------------------------------
{
	memset(mLevelFields, 0, sizeof(SearchLevelInfoT) * cMaxNumSearchLevels);
}

//-----------------------------------
CSearchManager::~CSearchManager(void)
//-----------------------------------
{
	AboutToClose();
	delete mAttributeItems;
}

//-----------------------------------
void CSearchManager::AboutToClose()
//-----------------------------------
{
	// Designed to be safe to call multiple times.
	Assert_(!IsSearching());
}

//-----------------------------------
OSErr CSearchManager::InitSearchManager(
	LView *inContainer, CSearchTabGroup *inTabGroup,
	MSG_ScopeAttribute inScope, LArray *inFolderScopeList )
//	Initialize the window. This class assumes that all level parameters are defined
//	within the paneID_ParamEncl view in the correct order.
//-----------------------------------
{
	OSErr error = noErr;
	
	StValueChanger<Boolean> change(mBroadcastUserChanges, false);
	
	Try_ {
		if ( inTabGroup ) inTabGroup->SetRotateWatchValue(&mCanRotateTarget);
		mParamEnclosure = USearchHelper::FindViewSubview(inContainer, paneID_ParamEncl);
		

		FindUIItemPtr( mParamEnclosure, paneID_More, mMoreButton );
		FindUIItemPtr( mParamEnclosure, paneID_Fewer, mFewerButton );
		FindUIItemPtr( mParamEnclosure, paneID_MatchAllRadio, mMatchAllRadio );
		FindUIItemPtr( mParamEnclosure, paneID_MatchAnyRadio, mMatchAnyRadio );

		// Link up this object to its sub broadcasters
		USearchHelper::LinkListenerToBroadcasters(mParamEnclosure, this);
		
		// Locate and cache references to the relevant search fields
		LArray *subPanes;
		LView *theView = dynamic_cast<LView *>(mParamEnclosure->FindPaneByID(paneID_ParamSubEncl));
		
		if ( theView != nil ) {
			subPanes = &theView->GetSubPanes();
			theView->SetRefreshAllWhenResized(false);
		} else {
			subPanes = &mParamEnclosure->GetSubPanes();
		}
		
		LArrayIterator iterator(*subPanes);
		
		AssertFail_(mNumLevels == 0);
		AssertFail_(mNumVisLevels == 0);
		
		LPane *pane;
		while ( iterator.Next(&pane) )
		{			
			Int32 userCon = pane->GetUserCon() - 1;
			Boolean updateNumLevels = true;
			PaneIDT thePane = pane->GetPaneID();
			switch ( thePane )
			{
				case paneID_Attributes:
					AssertFail_(IsBetween(userCon, 0, cMaxNumSearchLevels - 1));
					mLevelFields[userCon].attributesPopup = dynamic_cast<CSearchPopupMenu *>(pane);
					break;
				case paneID_Operators:
					AssertFail_(IsBetween(userCon, 0, cMaxNumSearchLevels - 1));
					mLevelFields[userCon].operatorsPopup = dynamic_cast<CSearchPopupMenu *>(pane);
					break;
				case paneID_TextValue:
					AssertFail_(IsBetween(userCon, 0, cMaxNumSearchLevels - 1));
					mLevelFields[userCon].valueText = dynamic_cast<CSearchEditField *>(pane);
					break;
				case paneID_IntValue:
					AssertFail_(IsBetween(userCon, 0, cMaxNumSearchLevels - 1));
					mLevelFields[userCon].valueInt = dynamic_cast<CSearchEditField *>(pane);
					break;
				case paneID_PopupValue:
					AssertFail_(IsBetween(userCon, 0, cMaxNumSearchLevels - 1));
					mLevelFields[userCon].valuePopup = dynamic_cast<CSearchPopupMenu *>(pane);
					break;
				case paneID_DateValue:
					AssertFail_(IsBetween(userCon, 0, cMaxNumSearchLevels - 1));
					mLevelFields[userCon].valueDate = dynamic_cast<CSearchDateField *>(pane);
					break;
				default:
					updateNumLevels = false;
					// Pass to next pane
					break;
			} // switch
			if ( updateNumLevels )
			{
				if ( userCon >= mNumLevels )
				{
					mNumLevels = userCon + 1;
					mNumVisLevels = mNumLevels;
				}
			}
		} // while
#ifdef Debug_Signal
		// Validate all the fields
		AssertFail_(mNumLevels > 0);
		for (Int32 i = 0; i < mNumLevels; ++i)
		{
			AssertFail_((mLevelFields[i].attributesPopup != nil) && (mLevelFields[i].operatorsPopup != nil) &&
						(mLevelFields[i].valueText != nil) && (mLevelFields[i].valuePopup != nil) &&
						(mLevelFields[i].valueDate != nil) && (mLevelFields[i].valueInt != nil) );
		}
#endif // gDebugSignal
		
		mParamEnclosure->SetRefreshAllWhenResized(false);
		
		// Find the margin between search levels
		if ( mNumLevels > 1 )
		{
			Rect portFrame;
			mLevelFields[1].attributesPopup->CalcPortFrameRect(portFrame);
			mLevelResizeV = portFrame.bottom;
			mLevelFields[0].attributesPopup->CalcPortFrameRect(portFrame);
			mLevelResizeV -= portFrame.bottom;
		}
		SetSearchScope(inScope, inFolderScopeList); // 97/03/03 second param was NULL.
	}
	Catch_(inErr)
	{
		error = inErr;
	}
	EndCatch_

	return error;
} // CSearchManager::InitSearchManager

//-----------------------------------
Boolean CSearchManager::CanSearch(void) const
//-----------------------------------
{
	if ( IsSearching() ) return false;
	
	for (Int32 i = 0; i < mNumVisLevels; ++i) 
	{
		if ( (USearchHelper::GetEditFieldLength(mLevelFields[i].valueText) < 1) && mLevelFields[i].valueText->IsVisible() ) 
			return false;
		}
	return true;
}


/*======================================================================================
	React to scope message.
======================================================================================*/

void CSearchManager::SetSearchScope(MSG_ScopeAttribute inScope, LArray *inFolderScopeList)
{
	// Set the scope and the list of folders selected. Currently only one
	// selection at the time is available - 11/19/97
	
	StValueChanger<Boolean> change(mBroadcastUserChanges, false);
	
	mCurrentScope = inScope;
	mFolderScopeList = inFolderScopeList;
	
	PopulateAttributesMenus(mCurrentScope);
	
	for (Int32 i = 0; i < mNumLevels; ++i) 
	{
//		SearchLevelInfoT *levelInfo = &mLevelFields[inAttributesMenu->GetUserCon() - 1];
		MessageAttributes(mLevelFields[i]);
	}
}


/*======================================================================================
	Set the number of visible levels.
======================================================================================*/

void CSearchManager::SetNumVisibleLevels(Int32 inNumLevels) {

	Int16 resizeAmount;
	
	{	// Begin scope for stack-based classes.
		
		StValueChanger<Boolean> change(mBroadcastUserChanges, false);
		StValueChanger<Boolean> change2(mIsBroadcasting, false);
		
		if ( inNumLevels < 1 ) {
			inNumLevels = 1;
		} else if ( inNumLevels > mNumLevels ) {
			inNumLevels = mNumLevels;
		}
		Int32 delta = inNumLevels - mNumVisLevels;
		
		Boolean more = (delta < 0) ? false : true;
		
		SDimension16 startFrameSize;
		mParamEnclosure->GetFrameSize(startFrameSize);

		// Hide or show the levels
		delta = abs( delta );
		while ( delta != 0 ) 
		{
			MessageMoreFewer( more );
				--delta;
			}

		SDimension16 endFrameSize;
		mParamEnclosure->GetFrameSize(endFrameSize);
		resizeAmount = endFrameSize.height - startFrameSize.height;
		
	}	// End scope for stack-based classes.

	
	BroadcastMessage(msg_SearchParametersResized, &resizeAmount);
}


/*======================================================================================
	React to stop message. Pass in a valid context if the user cancelled.
======================================================================================*/

void CSearchManager::StopSearch(MWContext *inContext) {

	if ( !IsSearching() ) return;
	
	mIsSearching = false;

	if ( inContext ) {
		Int32 rtnVal = MSG_InterruptSearch(inContext);
	}

	mParamEnclosure->Enable();
}


/*======================================================================================
	React to save message.
======================================================================================*/

/* Not implemented
void CSearchManager::SaveSearchResults(void)
{
}
*/

/*======================================================================================
	Adjust the search parameters to saved status data.
======================================================================================*/

void CSearchManager::ReadSavedSearchStatus(LStream *inStatusData)
{

	StValueChanger<Boolean> change(mBroadcastUserChanges, false);

	if ( inStatusData == nil )
		SetNumVisibleLevels(1);
	else
	{
		Int32 numVisibleLevels;
		*inStatusData >> numVisibleLevels;

		SetNumVisibleLevels(numVisibleLevels);
		
		Int32 numLevels;
		*inStatusData >> numLevels;
		*inStatusData >> mBoolOperator;
		
		(mBoolOperator ? mMatchAllRadio : mMatchAnyRadio)->SetValue(Button_On);

		SearchLevelInfoT *curLevel = mLevelFields;
		for (int i = 0; i < numLevels; i++)
		{
			CommandT command1, 	command2;
			SearchTextValueT 	text;
			long				age;
			Str255				ageStr;
			
			*inStatusData >> command1;
			*inStatusData >> command2;
			*inStatusData >> ((StringPtr) text.text);
			*inStatusData >> age;
			// be cautious: if some future version supports a larger number of levels,
			// just skip the data, don't do anything with it.  This caused a crash
			// when the search window was used in 4.01 after running it in 4.02 (7/30 build). 
			if (i < mNumLevels)
			{
				curLevel->attributesPopup->SetCurrentItemByCommand(command1);
				curLevel->operatorsPopup->SetCurrentItemByCommand(command2);
				curLevel->valueText->SetDescriptor((StringPtr) text.text);
				::NumToString( age, ageStr );
				curLevel->valueInt->SetDescriptor( ageStr );
				curLevel++;
			}
					
		}
	}
}


Boolean				
CSearchManager::GetBooleanOperator() const
{
	return mBoolOperator;
}

/*======================================================================================
	Get the search status to save.
======================================================================================*/

void CSearchManager::WriteSavedSearchStatus(LStream *outStatusData)
{

	*outStatusData << mNumVisLevels;
	*outStatusData << mNumLevels;
	*outStatusData << mBoolOperator;
	
	SearchLevelInfoT *curLevel = mLevelFields; 
	SearchLevelInfoT *endLevel = curLevel + mNumLevels;
	
	SearchTextValueT 		text;
	Str255					ageStr;
	long					age;
	
	do {
		*outStatusData << curLevel->attributesPopup->GetCurrentItemCommandNum();			
		*outStatusData << curLevel->operatorsPopup->GetCurrentItemCommandNum();
		*outStatusData << curLevel->valueText->GetDescriptor((StringPtr) text.text);
		curLevel->valueInt->GetDescriptor( ageStr );
		::StringToNum(ageStr, &age);
		*outStatusData << age;
	} while ( ++curLevel < endLevel );
}


/*======================================================================================
	Get the current search parameters. outSearchParams must be able to hold at least
	GetNumVisibleLevels() elements. Strings are output as c-strings.
======================================================================================*/

void CSearchManager::GetSearchParameters(SearchLevelParamT *outSearchParams) {

	SearchLevelInfoT *curLevel = mLevelFields;
	SearchLevelInfoT *endLevel = curLevel + mNumVisLevels;

	do {
		CommandT attributeCommand = curLevel->attributesPopup->GetCurrentItemCommandNum();
		outSearchParams->val.attribute = ((MSG_SearchAttribute) attributeCommand );
		
		// *** if custom header get the the menu location and save it
		if( attributeCommand == attribOtherHeader	)
			outSearchParams->customHeaderPos = curLevel->attributesPopup->GetValue();
		else
			outSearchParams->customHeaderPos = 0;
						
		outSearchParams->op = (MSG_SearchOperator) 
			curLevel->operatorsPopup->GetCurrentItemCommandNum();
		
		outSearchParams->boolOp = mBoolOperator;

		MSG_SearchValueWidget widget;
		FailSearchError(MSG_GetSearchWidgetForAttribute(outSearchParams->val.attribute, &widget));
		
		switch ( widget ) {
			case widgetText: {
					if ( curLevel->valueText->IsVisible() ) {
						curLevel->valueText->GetDescriptor((StringPtr) outSearchParams->val.u.string);
						if ( outSearchParams->val.u.string[0] == 0 ) {
							outSearchParams->op = opIsEmpty;
						} else {
							P2CStr((StringPtr) outSearchParams->val.u.string);
						}
					} else {
						outSearchParams->op = opIsEmpty;
						*outSearchParams->val.u.string = '\0';
					}
				}
				break;
				
			case widgetInt:
					if ( curLevel->valueInt->IsVisible() ) 
					{
						Str255		age;
						curLevel->valueInt->GetDescriptor( age );
						if ( age[0] == 0 ) 
							outSearchParams->op = opIsEmpty;
						else 
						{
							Int32	ageNum;
							::StringToNum( age, &ageNum);
							outSearchParams->val.u.age = ageNum;
						}
					} 
					else 
					{
						outSearchParams->op = opIsEmpty;
						outSearchParams->val.u.age = 0;
					}
				break;
				
			case widgetDate: {
					AssertFail_(curLevel->valueDate->IsVisible());
					Int16 year; UInt8 month, day;
					curLevel->valueDate->GetDate(&year, &month, &day);
					tm time;
					
					time.tm_sec = 1;
					time.tm_min = 1;
					time.tm_hour = 1;
					time.tm_mday = day;
					time.tm_mon = month - 1;
					time.tm_year = year - 1900;
					time.tm_wday = -1;
					time.tm_yday = -1;
					time.tm_isdst = -1;
					
					outSearchParams->val.u.date = ::mktime(&time);
#ifdef Debug_Signal
					tm *checkTime = ::localtime(&outSearchParams->val.u.date);
					Assert_((time.tm_sec == checkTime->tm_sec) && (time.tm_min == checkTime->tm_min) &&
							(time.tm_hour == checkTime->tm_hour) && (time.tm_mday == checkTime->tm_mday) &&
							(time.tm_mon == checkTime->tm_mon) && (time.tm_year == checkTime->tm_year));
#endif // Debug_Signal
				}
				break;
			
			case widgetMenu: {
					AssertFail_(curLevel->valuePopup->IsVisible());
					if ( outSearchParams->val.attribute == attribPriority ) {
						outSearchParams->val.u.priority = (MSG_PRIORITY) 
							curLevel->valuePopup->GetCurrentItemCommandNum();
					} else {
						AssertFail_(outSearchParams->val.attribute == attribMsgStatus);
						outSearchParams->val.u.msgStatus = (uint32) 
							curLevel->valuePopup->GetCurrentItemCommandNum();
					}
				}
				break;
			
			default:
				FailOSErr_(paramErr);	// Should never get here!
				break;
		}
		++outSearchParams;
	} while ( ++curLevel < endLevel );
}


/*======================================================================================
	Set the current search parameters. inSearchParams must hold at least inNumVisLevels
	elements. If inNumVisLevels < GetNumLevels(), the remaining levels are set
	to their cleared values. The InitSearchManager() method must be called before calling
	this method. Set inSearchParams to nil to clear all the values. Strings are input
	as c-strings.
======================================================================================*/

void CSearchManager::SetSearchParameters(Int16 inNumVisLevels, const SearchLevelParamT *inSearchParams) {

	AssertFail_((mNumVisLevels > 0) && (mNumLevels > 0) && IsBetween(inNumVisLevels, 1, mNumLevels));
	StValueChanger<Boolean> change(mBroadcastUserChanges, false);
	
	SetNumVisibleLevels(inNumVisLevels);
	
	AssertFail_(inNumVisLevels == mNumVisLevels);
	
	if ( inSearchParams != nil ) {
		SearchLevelInfoT *curLevel = mLevelFields, *endLevel = curLevel + mNumVisLevels;
		const SearchLevelParamT *curParam = inSearchParams;
		do {
			curLevel->attributesPopup->SetCurrentItemByCommand(curParam->val.attribute);
			curLevel->operatorsPopup->SetCurrentItemByCommand(curParam->op);
			
			MSG_SearchValueWidget widget;
			FailSearchError(MSG_GetSearchWidgetForAttribute(curParam->val.attribute, &widget));
			switch ( widget ) {
				case widgetText: 
					{
//						if (curParam->op == opIsEmpty)			// 
//							*curParam->val.u.string = '\0';		// 
						CStr255 string(curParam->val.u.string);
						if ( string.Length() == 0 ) 
							curLevel->operatorsPopup->SetCurrentItemByCommand(opIsEmpty);
						curLevel->valueText->SetDescriptor(string);
					}
					curLevel->valueDate->SetToToday();
					curLevel->valuePopup->SetToDefault();
					break;
				
				case widgetInt: 
					if( curParam->val.u.age == 0 )
					{
						curLevel->operatorsPopup->SetCurrentItemByCommand(opIsEmpty);
						curLevel->valueInt->SetDescriptor( CStr255("") );
					}
					else
					{
						Str255	ageString;
						::NumToString( curParam->val.u.age, ageString);
						curLevel->valueInt->SetDescriptor( ageString );
					}
					curLevel->valueDate->SetToToday();
					curLevel->valuePopup->SetToDefault();
					break;
					
				case widgetDate: {
						tm *time = ::localtime(&curParam->val.u.date);
						AssertFail_(time != nil);
#ifdef Debug_Signal
						time_t checkTime = ::mktime(time);
						Assert_(checkTime == curParam->val.u.date);
#endif // Debug_Signal
						curLevel->valueDate->SetDate(time->tm_year + 1900, time->tm_mon + 1, time->tm_mday);
					}
					curLevel->valueText->SetDescriptor("\p"); 
					curLevel->valuePopup->SetToDefault();
					break;
					
				case widgetMenu:
					if ( curParam->val.attribute == attribPriority ) {
						curLevel->valuePopup->SetCurrentItemByCommand(curParam->val.u.priority);
					} else {
						AssertFail_(curParam->val.attribute == attribMsgStatus);	// Only other option!
						curLevel->valuePopup->SetCurrentItemByCommand(curParam->val.u.msgStatus);
					}
					curLevel->valueText->SetDescriptor("\p"); 
					curLevel->valueDate->SetToToday();
					break;
					
				default:
					AssertFail_(false);	// Should never get here!
					break;
			}
			++curParam;
		} while ( ++curLevel < endLevel );
	} else {
		inNumVisLevels = 0;
	}
	
	if ( inNumVisLevels < mNumLevels ) {
		ClearSearchParams(inNumVisLevels + 1, mNumLevels);
	}
}




#if 0
/*======================================================================================
	If the returned MWContext is non-nil, the specified row is currently open and the
	returned context belongs to the opened pane. This method always return the context
	type of the specified result element.
======================================================================================*/

MWContext *CSearchManager::IsResultElementOpen(MSG_ViewIndex inIndex, MWContextType *outType) {

	UpdateMsgResult(inIndex);
	
	MWContext *rtnContext = MSG_IsResultElementOpen(mResultElement);
	*outType = MSG_GetResultElementType(mResultElement);

	return rtnContext;
}

#endif



/*======================================================================================
	Populate the priority menu.
======================================================================================*/

void CSearchManager::PopulatePriorityMenu(CSearchPopupMenu *inMenu) {

	// Call BE to get menu items
	
	if ( (mLastMenuItemsScope != (MSG_ScopeAttribute) cPriorityScope) || (mLastMenuItemsAttribute != kNumAttributes) ) {
		MSG_SearchMenuItem *curMenuItem = mSearchMenuItems;
		for (Int32 i = MSG_LowestPriority; i <= (Int32) MSG_HighestPriority; ++i, ++curMenuItem) {
			MSG_GetPriorityName((MSG_PRIORITY) i, curMenuItem->name, sizeof(curMenuItem->name));
			C2PStr(curMenuItem->name);
			curMenuItem->attrib = i;
			curMenuItem->isEnabled = true;
		}
		mNumLastMenuItems = curMenuItem - mSearchMenuItems;
		mLastMenuItemsScope = (MSG_ScopeAttribute) cPriorityScope;
		mLastMenuItemsAttribute = kNumAttributes;
	}
	
	UInt32 populateID = 0x7000000;
	inMenu->PopulateMenu(mSearchMenuItems, mNumLastMenuItems, MSG_NormalPriority, populateID);
}


/*======================================================================================
	Populate the status menu.
======================================================================================*/

void CSearchManager::PopulateStatusMenu(CSearchPopupMenu *inMenu) {

	const int32 cStatusValues[] = { MSG_FLAG_READ,
									MSG_FLAG_REPLIED,
									MSG_FLAG_FORWARDED };

	uint16 nStatus = sizeof(cStatusValues) / sizeof(int32);

	// Call BE to get menu items
	
	if ( (mLastMenuItemsScope != (MSG_ScopeAttribute) cStatusScope) || (mLastMenuItemsAttribute != kNumAttributes) ) {
		MSG_SearchMenuItem *curMenuItem = mSearchMenuItems;
		for (Int32 i = 0; i < nStatus && i < cMaxNumSearchMenuItems; ++i) {
			Int32 status = cStatusValues[i];
			MSG_GetStatusName(status, curMenuItem->name, sizeof(curMenuItem->name));
			C2PStr(curMenuItem->name);
			if ( curMenuItem->name[0] > 0 ) {
				curMenuItem->attrib = status;
				curMenuItem->isEnabled = true;
				++curMenuItem;
			}
		}
		mNumLastMenuItems = curMenuItem - mSearchMenuItems;
		mLastMenuItemsScope = (MSG_ScopeAttribute) cStatusScope;
		mLastMenuItemsAttribute = kNumAttributes;
	}
	
	UInt32 populateID = 0x6000000;
	inMenu->PopulateMenu(mSearchMenuItems, mNumLastMenuItems, MSG_FLAG_READ, populateID);
}


/*======================================================================================
	Respond to broadcaster messages.
======================================================================================*/

void CSearchManager::ListenToMessage(MessageT inMessage, void *ioParam) {

	Boolean userChanged = true;

	switch ( inMessage ) {
	
		case paneID_Attributes:
			CSearchPopupMenu*	currentSearchPopup = static_cast<CSearchPopupMenu*>( ioParam );
			
			// Check if the attribute is the customize command.
			// If it is execute it.
			CommandT command = currentSearchPopup->GetCurrentItemCommandNum( currentSearchPopup->GetValue() );
			if( command == eCustomizeSearchItem )
			{
				StValueChanger<Boolean> change(mBroadcastUserChanges, false); // stop broadcasting inside this scope
				EditCustomHeaders();
				userChanged = false;
			}
			else
			{
				MessageAttributes( mLevelFields[currentSearchPopup->GetUserCon() - 1] );
				if ( mBroadcastUserChanges ) 
					SelectSearchLevel(((CSearchPopupMenu *) ioParam)->GetUserCon(), eCheckNothing);
			}
			break;
			
		case paneID_Operators:
			MessageOperators((CSearchPopupMenu *) ioParam);
			if ( mBroadcastUserChanges ) SelectSearchLevel(((CSearchPopupMenu *) ioParam)->GetUserCon(), eCheckNothing);
			break;

		case msg_AND_OR_Toggle:
			mBoolOperator = (mMatchAllRadio->GetValue() == Button_On ? true : false);	
			break;
			
		case paneID_More:
		case paneID_Fewer: {
				StValueChanger<Boolean> change(mCanRotateTarget, false);
				Int32 curSelectedLevel = GetSelectedSearchLevel();
				MessageMoreFewer(inMessage == paneID_More);
				if ( mBroadcastUserChanges ) {
					if ( (inMessage == paneID_More) || !curSelectedLevel || 
						 (curSelectedLevel == (mNumVisLevels + 1)) ) {
						SelectSearchLevel(mNumVisLevels, eCheckBackward);
					}
				}
			}
			break;

		case paneID_Clear:
			MessageClear();
			if ( mBroadcastUserChanges ) SelectSearchLevel(1, eCheckForward);
			break;
			
		case CSearchEditField::msg_UserChangedText:
			break;
			
		default:
			userChanged = false;
			// Nothing to do, no inherited method
			break;
	}
	
	if ( userChanged ) UserChangedParameters();
}


//-----------------------------------
MSG_ScopeAttribute CSearchManager::GetBEScope(MSG_ScopeAttribute inScope) const
//	Convert the current search scope as we store it in this manager to a scope that is
//	understood by the BE.
//-----------------------------------
{
	MSG_ScopeAttribute beScope = scopeMailFolder;
	
	if ( (inScope == scopeNewsgroup) || (inScope == cScopeNewsSelectedItems) )
		beScope = scopeNewsgroup;
	else if ( inScope == scopeLdapDirectory )
		beScope = scopeLdapDirectory;
	return beScope;
} // CSearchManager::GetBEScope

//-----------------------------------
void CSearchManager::PopulateAttributesMenus(MSG_ScopeAttribute inScope)
//	Populate the attributes menus according to the specified scope.
//-----------------------------------
{
	MSG_ScopeAttribute beScope = GetBEScope(inScope);
	
	// Call BE to get menu items

	
	if ( (mLastMenuItemsScope != beScope) || (mLastMenuItemsAttribute != kNumAttributes) )
	{
		// mNumLastMenuItems = cMaxNumSearchMenuItems;
		void* scopeItem = nil;
		
		if (!mFolderScopeList)
		{
			// This happens with filters.  Get a folder info for the inbox.
			MSG_FolderInfo* inbox;
			::MSG_GetFoldersWithFlag(
				CMailNewsContext::GetMailMaster(),
				MSG_FOLDER_FLAG_INBOX,
				&inbox,
				1);
			scopeItem = inbox;
		}
		else if (!mFolderScopeList->FetchItemAt(LArray::index_First, &scopeItem))
			return;

		if (scopeItem)
		{
			// *** get number of items
			mLastNumOfAttributes = GetNumberOfAttributes( beScope, scopeItem );
		
			if (mFolderScopeList)
				FailSearchError(::MSG_GetAttributesForSearchScopes(
					CMailNewsContext::GetMailMaster(),
					beScope,
					&scopeItem,
					1,
					mAttributeItems,
					&mLastNumOfAttributes));
			else
				FailSearchError(::MSG_GetAttributesForFilterScopes(
					CMailNewsContext::GetMailMaster(),
					beScope,
					&scopeItem,
					1,
					mAttributeItems,
					&mLastNumOfAttributes));
			
			// Finish creating the attributes menu for the filter and message search
			if( beScope != scopeLdapDirectory ) 
				FinishCreateAttributeMenu( mAttributeItems, mLastNumOfAttributes );
					
			mLastMenuItemsScope = beScope;
			mLastMenuItemsAttribute = kNumAttributes;
			
			// Convert to pascal string
			for (Int32 j = 0; j < mLastNumOfAttributes; ++j)
				C2PStr(mAttributeItems[j].name);
				
			//UInt32 populateID = ((((UInt32) beScope)<<16) | ((UInt32) 0) | 0x4000000);
			UInt32 populateID = (UInt32)scopeItem;
			for (Int32 i = 0; i < mNumLevels; ++i)
			{
				mLevelFields[i].attributesPopup->PopulateMenu(
					mAttributeItems, mLastNumOfAttributes, attribSender, populateID);
			}
		}
	}													 
} // CSearchManager::PopulateAttributesMenus

//-----------------------------------
void CSearchManager::PopulateOperatorsMenus(MSG_ScopeAttribute inScope)
//	Populate the operators menus according to the specified scope. This method assumes
//	that the attributes menus have been setup to the scope already by calling the
//	PopulateAttributesMenus() method.
//-----------------------------------
{
	MSG_ScopeAttribute beScope = GetBEScope(inScope);

	// Call BE to get menu items
	
	MSG_SearchAttribute lastAttrib = kNumAttributes;
	CommandT newCommandIfInvalid;
	UInt32 populateID;
	
	void* scopeItem = nil;
	if (!mFolderScopeList)
	{
		// This happens with filters.  Get a folder info for the inbox.
		MSG_FolderInfo* inbox;
		::MSG_GetFoldersWithFlag(
			CMailNewsContext::GetMailMaster(),
			MSG_FOLDER_FLAG_INBOX,
			&inbox,
			1);
		scopeItem = inbox;
	}
	else if (!mFolderScopeList->FetchItemAt(LArray::index_First, &scopeItem))
		return;
	if (!scopeItem)
		return;
	for (Int32 i = 0; i < mNumLevels; ++i)
	{
		MSG_SearchAttribute attrib
			= (MSG_SearchAttribute)mLevelFields[i].attributesPopup->GetCurrentItemCommandNum();
		if ( lastAttrib != attrib )
		{
			if ( (mLastMenuItemsScope != beScope) || (mLastMenuItemsAttribute != attrib) )
			{
				mNumLastMenuItems = cMaxNumSearchMenuItems;
				if (mFolderScopeList)
					FailSearchError(MSG_GetOperatorsForSearchScopes(
						CMailNewsContext::GetMailMaster(),
						beScope,
						&scopeItem,
						1,
						attrib,
						mSearchMenuItems,
						&mNumLastMenuItems));
				else
					FailSearchError(MSG_GetOperatorsForFilterScopes(
						CMailNewsContext::GetMailMaster(),
						beScope,
						&scopeItem,
						1,
						attrib,
						mSearchMenuItems,
						&mNumLastMenuItems));
				mLastMenuItemsScope = beScope;
				mLastMenuItemsAttribute = attrib;
				// Convert to pascal string
				for (Int32 j = 0; j < mNumLastMenuItems; ++j)
					C2PStr(mSearchMenuItems[j].name);
			}
			lastAttrib = attrib;
			
			MSG_SearchValueWidget widget;
			FailSearchError(MSG_GetSearchWidgetForAttribute(lastAttrib, &widget));
			
			switch ( widget ) {
				case widgetText: 	newCommandIfInvalid = opContains; break;
				case widgetDate:	newCommandIfInvalid = opIsBefore; break;
				case widgetMenu:	newCommandIfInvalid = opIs; break;
				case widgetInt:		newCommandIfInvalid = opIsGreaterThan; break;
				default:
					AssertFail_(false);	// Should never get here!
					newCommandIfInvalid = 0;
					break;
			}
			//populateID = ((((UInt32) beScope)<<16) | ((UInt32) lastAttrib) | 0x5000000);
			populateID = ((((UInt32) scopeItem)<<16) | ((UInt32) lastAttrib) | 0x5000000);
		}
		mLevelFields[i].operatorsPopup->PopulateMenu(
			mSearchMenuItems, mNumLastMenuItems,
			newCommandIfInvalid, populateID);
	} // for
} // CSearchManager::PopulateOperatorsMenus

/*======================================================================================
	Select the active input search field in the specified search parameter level.
======================================================================================*/

void CSearchManager::SelectSearchLevel(Int16 inBeginLevel, ECheckDirection inCheckDirection) {

	if ( !IsBetween(inBeginLevel, 1, mNumVisLevels) ) return;
	Int32 inc, end;
	
	switch ( inCheckDirection ) {
		case eCheckForward:		inc = 1; end = mNumVisLevels + 1; break;
		case eCheckBackward:	inc = -1; end = 0; break;
		default:				inc = 1; end = inBeginLevel + 1; break;
	}
	
	for (Int32 i = inBeginLevel; i != end; i += inc) {
		SearchLevelInfoT *levelInfo = &mLevelFields[i - 1];
		MSG_SearchAttribute attrib = (MSG_SearchAttribute) levelInfo->attributesPopup->GetCurrentItemCommandNum();
		MSG_SearchValueWidget widget;
		FailSearchError(MSG_GetSearchWidgetForAttribute(attrib, &widget));

		if ( widget == widgetText ) 
		{
			Assert_(levelInfo->valueText->IsLatentVisible());
			USearchHelper::SelectEditField(levelInfo->valueText);
			return;
		} 
		else if ( widget == widgetDate ) 
		{
			Assert_(levelInfo->valueDate->IsLatentVisible());
			USearchHelper::SelectDateView(levelInfo->valueDate);
			return;
		}
		else if( widget == widgetInt )
		{
			Assert_(levelInfo->valueInt->IsLatentVisible());
			USearchHelper::SelectEditField(levelInfo->valueInt);
			return;
		}
	}
}
/*======================================================================================
	Set the wincsid for the editfield
======================================================================================*/

void CSearchManager::SetWinCSID(int16 wincsid) {
	ResIDT textTraitsID = 8603;
	if( wincsid != INTL_CharSetNameToID(INTL_ResourceCharSet()) )
		textTraitsID = CPrefs::GetTextFieldTextResIDs(wincsid);	

	for (Int32 i = 0; i < mNumVisLevels ; i++) {
		SearchLevelInfoT *levelInfo = &mLevelFields[i];
		MSG_SearchAttribute attrib = (MSG_SearchAttribute) levelInfo->attributesPopup->GetCurrentItemCommandNum();
		if( attrib == eCustomizeSearchItem )
			attrib = (MSG_SearchAttribute) levelInfo->attributesPopup->GetOldItemCommand();
			
		// *******************************************************************************
		//
		// !!! HACK ALERT !!!
		//
		// The current search architecture assumes that the location of the menu item correspond to the 
		// command number. Bad assumption. The "Age in Days" attribute does not follow this system. The
		// following code is a hack to fix this problem. It checks for the item number and current scope
		// and substitutes the correct command for the bad assumption.
		// 
		// 12/3/97
		//
		// *******************************************************************************
		
		if( mCurrentScope != scopeLdapDirectory && attrib == attribCommonName )
			attrib = attribAgeInDays;
			
		// *******************************************************************************
				
		MSG_SearchValueWidget widget;
		FailSearchError(MSG_GetSearchWidgetForAttribute(attrib, &widget));

		if ( widget == widgetText ) 
		{
			Assert_(levelInfo->valueText->IsLatentVisible());
			levelInfo->valueText->SetTextTraitsID(textTraitsID);
			levelInfo->valueText->Refresh();
		} 
		else if ( widget == widgetInt ) 
		{
			Assert_(levelInfo->valueInt->IsLatentVisible());
			levelInfo->valueInt->SetTextTraitsID(textTraitsID);
			levelInfo->valueInt->Refresh();
		}
	}
}


/*======================================================================================
	Find the currently selected (i.e. active user input) search level, 0 if none.
======================================================================================*/

Int32 CSearchManager::GetSelectedSearchLevel(void) {

	SearchLevelInfoT *curlevelInfo = &mLevelFields[0];
	SearchLevelInfoT *endlevelInfo = curlevelInfo + mNumVisLevels;
	
	while ( curlevelInfo < endlevelInfo ) {
		if ( curlevelInfo->valueText->IsTarget() || curlevelInfo->valueDate->ContainsTarget() 
				|| curlevelInfo->valueInt->IsTarget() ) 
		{
			return (curlevelInfo - &mLevelFields[0] + 1);
		}
		++curlevelInfo;
	}
	return 0;
}


/*======================================================================================
	React to attributes message.
======================================================================================*/

//void CSearchManager::MessageAttributes(CSearchPopupMenu *inAttributesMenu) {
void CSearchManager::MessageAttributes( SearchLevelInfoT& searchLevel ) {

//	SearchLevelInfoT *levelInfo = &mLevelFields[inAttributesMenu->GetUserCon() - 1];
	CSearchPopupMenu* inAttributesMenu = searchLevel.attributesPopup;
	
	PopulateOperatorsMenus(mCurrentScope);
	
	// Find out which value type to display
	
	MSG_SearchAttribute attrib = (MSG_SearchAttribute) inAttributesMenu->GetCurrentItemCommandNum();
	MSG_SearchValueWidget widget;
	FailSearchError(MSG_GetSearchWidgetForAttribute(attrib, &widget));
	
	if ( widget == widgetMenu ) {
		if ( attrib == attribPriority ) {
			PopulatePriorityMenu(searchLevel.valuePopup);
		} else {
			PopulateStatusMenu(searchLevel.valuePopup);
		}
	}
	
	if ( inAttributesMenu->IsLatentVisible() ) {
	// !!!
		USearchHelper::ShowHidePane(searchLevel.valueText, widget == widgetText);
		USearchHelper::ShowHidePane(searchLevel.valueInt, widget == widgetInt);
		USearchHelper::ShowHidePane(searchLevel.valueDate, widget == widgetDate);
		USearchHelper::ShowHidePane(searchLevel.valuePopup, widget == widgetMenu);
	} else {
		switch ( widget ) 
		// !!! need to add support for days
		{
			case widgetText: 	searchLevel.lastVisibleValue = searchLevel.valueText; break;
			case widgetInt: 	searchLevel.lastVisibleValue = searchLevel.valueInt; break;
			case widgetDate:	searchLevel.lastVisibleValue = searchLevel.valueDate; break;
			case widgetMenu:	searchLevel.lastVisibleValue = searchLevel.valuePopup; break;
			default:
				AssertFail_(false);	// Should never get here!
				break;
		}
	}

	MessageOperators(searchLevel.operatorsPopup);
}


/*======================================================================================
	React to operators message.
======================================================================================*/

void CSearchManager::MessageOperators(CSearchPopupMenu *inOperatorsMenu) {

	SearchLevelInfoT *levelInfo = &mLevelFields[inOperatorsMenu->GetUserCon() - 1];
	
	MSG_SearchOperator op = (MSG_SearchOperator) inOperatorsMenu->GetCurrentItemCommandNum();
	MSG_SearchAttribute attrib = (MSG_SearchAttribute) levelInfo->attributesPopup->GetCurrentItemCommandNum();
	MSG_SearchValueWidget widget;
	FailSearchError(MSG_GetSearchWidgetForAttribute(attrib, &widget));
	
	if ( widget == widgetText ) 
	{
		if ( op == opIsEmpty ) levelInfo->lastVisibleValue = nil;
		if ( inOperatorsMenu->IsLatentVisible() ) 
			USearchHelper::ShowHidePane(levelInfo->valueText, op != opIsEmpty);
	} 
	else if (  widget == widgetInt )
	{
		if ( op == opIsEmpty ) levelInfo->lastVisibleValue = nil;
		if ( inOperatorsMenu->IsLatentVisible() ) 
			USearchHelper::ShowHidePane(levelInfo->valueInt, op != opIsEmpty);
	} 
}


/*======================================================================================
	React to more message.
======================================================================================*/

void CSearchManager::MessageMoreFewer(Boolean inMore) {

	AssertFail_((mNumLevels > 0) && (mNumLevels <= cMaxNumSearchLevels));
	AssertFail_((mNumVisLevels > 0) && (mNumVisLevels <= mNumLevels));
	AssertFail_(!(inMore && (mNumVisLevels == mNumLevels)) || (!inMore && (mNumVisLevels == 1)));

	// Show/hide subviews
	
	SearchLevelInfoT *showHideLevel = &mLevelFields[inMore ? mNumVisLevels : (mNumVisLevels - 1)];
	
	if ( !inMore ) {
		if ( showHideLevel->valuePopup->IsLatentVisible() ) {
			showHideLevel->lastVisibleValue = showHideLevel->valuePopup;
		} else if ( showHideLevel->valueText->IsLatentVisible() ) {
			showHideLevel->lastVisibleValue = showHideLevel->valueText;
		} else if ( showHideLevel->valueInt->IsLatentVisible() ) {
			showHideLevel->lastVisibleValue = showHideLevel->valueInt;
		} else if ( showHideLevel->valueDate->IsLatentVisible() ) {
			showHideLevel->lastVisibleValue = showHideLevel->valueDate;
		} else {
			showHideLevel->lastVisibleValue = nil;
		}
	}
	USearchHelper::ShowHidePane(showHideLevel->attributesPopup, inMore);
	USearchHelper::ShowHidePane(showHideLevel->operatorsPopup, inMore);
	
	if ( showHideLevel->lastVisibleValue != nil ) {
		USearchHelper::ShowHidePane(showHideLevel->lastVisibleValue, inMore);
	}
	
	// Resize the enclosing pane
	
	Int16 resizeAmount = inMore ? mLevelResizeV : -mLevelResizeV;
	mParamEnclosure->ResizeFrameBy(0, resizeAmount, true);

	// Enable/disable more/fewer
	
	mNumVisLevels += (inMore ? 1 : -1);
	USearchHelper::EnableDisablePane(mMatchAllRadio, (mNumVisLevels > 1));
	USearchHelper::EnableDisablePane(mMatchAnyRadio, (mNumVisLevels > 1));
	USearchHelper::EnableDisablePane(mMoreButton, (mNumVisLevels < mNumLevels));
	USearchHelper::EnableDisablePane(mFewerButton, (mNumVisLevels > 1));
		
	BroadcastMessage(msg_SearchParametersResized, &resizeAmount);
}


/*======================================================================================
	React to clear message. inStartLevel and inEndLevel are 1 based.
======================================================================================*/

void CSearchManager::MessageClear(void) {

	ClearSearchParams(1, mNumLevels);

	BroadcastMessage(msg_SearchParametersCleared);
}


/*======================================================================================
	Clear the search params for the specified levels.
======================================================================================*/

void CSearchManager::ClearSearchParams(Int16 inStartLevel, Int16 inEndLevel) {

	AssertFail_(IsBetween(inStartLevel, 1, mNumLevels));
	AssertFail_(IsBetween(inEndLevel, 1, mNumLevels));
	AssertFail_(inStartLevel <= inEndLevel);

	SearchLevelInfoT *curLevel = mLevelFields + inStartLevel - 1;
	SearchLevelInfoT *endLevel = mLevelFields + inEndLevel;

	do {
		curLevel->attributesPopup->SetToDefault();
		curLevel->operatorsPopup->SetToDefault();
		curLevel->valueText->SetDescriptor("\p");
		curLevel->valueInt->SetDescriptor( CStr255("") );
		curLevel->valueDate->SetToToday();
		curLevel->valuePopup->SetToDefault();
	} while ( ++curLevel < endLevel );
}




//-----------------------------------
void CSearchManager::AddScopesToSearch(MSG_Master *inMaster)
//-----------------------------------
{
	AssertFail_(mMsgPane != nil);	
	Boolean addedScope = false;
	switch (mCurrentScope)
	{
	case cScopeMailFolderNewsgroup:
		FailSearchError(MSG_AddAllScopes(mMsgPane, inMaster, scopeMailFolder));
		FailSearchError(MSG_AddAllScopes(mMsgPane, inMaster, scopeNewsgroup));
		addedScope = true;
		break;
	case scopeMailFolder:
	case scopeNewsgroup:
		FailSearchError(MSG_AddAllScopes(mMsgPane, inMaster, mCurrentScope));
		addedScope = true;
		break;
	case scopeLdapDirectory:
		{
			// In this case, mFolderScopeList contains only the single LDAP directory.
			LArrayIterator iterator(*mFolderScopeList);
			DIR_Server *dirServer;
			while ( iterator.Next(&dirServer) )
			{
				FailSearchError(MSG_AddLdapScope(mMsgPane, dirServer));
				addedScope = true; // in the loop, in case there are no servers!
			}
			break;
		}
	}
	if ( !addedScope )
	{
		// SOME scope must be specified!
		if ( (mFolderScopeList == nil) || (mFolderScopeList->GetCount() < 1) ||
			 ((mCurrentScope != cScopeMailSelectedItems) && (mCurrentScope != cScopeNewsSelectedItems)) )
		{
			FailOSErr_(paramErr);
		}
		LArrayIterator iterator(*mFolderScopeList);
		MSG_FolderInfo *msgFolderInfo;
		while ( iterator.Next(&msgFolderInfo) )
		{
			MSG_FolderLine folderLine;
			if( MSG_GetFolderLineById(inMaster,
										msgFolderInfo,
										&folderLine))
			{
				FailSearchError(
					MSG_AddScopeTerm(mMsgPane,
									(folderLine.flags & MSG_FOLDER_FLAG_MAIL) ? scopeMailFolder : scopeNewsgroup,
									msgFolderInfo) );
			}
		}
	}
} // CSearchManager::AddScopesToSearch

//-----------------------------------
void CSearchManager::AddParametersToSearch(void)
//-----------------------------------
{
	AssertFail_(mMsgPane != nil);

	// Get the search parameters
	StSearchDataBlock searchParams(mNumVisLevels, StSearchDataBlock::eAllocateStrings);
	GetSearchParameters(searchParams.GetData());
	
	// Add parameters to the search
	SearchLevelParamT *curLevel = searchParams.GetData();
	SearchLevelParamT *endLevel = curLevel + mNumVisLevels;
	do {
		char	*customHeader = nil; 
		
		// *** If the attribute is a custom header pass it in using the variable
		// customHeader, otherwise pass a nil value.
		if( curLevel->val.attribute == attribOtherHeader )
			customHeader = GetSelectedCustomHeader( curLevel );

		FailSearchError(MSG_AddSearchTerm(mMsgPane, curLevel->val.attribute, curLevel->op,
										  &curLevel->val, mBoolOperator, customHeader ));
										  
	} while ( ++curLevel < endLevel );
} // CSearchManager::AddParametersToSearch

//-----------------------------------
void CSearchManager::FailSearchError(MSG_SearchError inError)
//	Raise a relevant exception if MSG_SearchError is not SearchError_Success.
//-----------------------------------
{
	if ( inError == SearchError_Success ) return;
	
	switch ( inError )
	{	
		case SearchError_OutOfMemory:
			Throw_(memFullErr);
			break;

		case SearchError_NotImplemented:
			{
#ifdef Debug_Signal
				CStr255 errorString;
				USearchHelper::AssignUString(
					USearchHelper::uStr_SearchFunctionNotImplemented, errorString);
				ErrorManager::PlainAlert((StringPtr) errorString);
#endif // Debug_Signal
				Throw_(unimpErr);
			}
			break;

		case SearchError_InvalidPane:		// probably searching a server with no subscribed groups			
			CStr255 errorString;
			USearchHelper::AssignUString(USearchHelper::uStr_SearchFunctionNothingToSearch, errorString);
			ErrorManager::PlainAlert((StringPtr) errorString);
			Throw_(userCanceledErr);
			break;
			
		case SearchError_NullPointer:
		case SearchError_ScopeAgreement:
		case SearchError_ListTooSmall:
		case SearchError_InvalidAttribute:
		case SearchError_InvalidScope:
		case SearchError_InvalidOperator:
		case SearchError_InvalidSearchTerm:
		case SearchError_InvalidScopeTerm:
		case SearchError_InvalidResultElement:
		case SearchError_InvalidStream:
		case SearchError_InvalidFolder: {
#ifdef Debug_Signal
				CStr255 errorString;
				USearchHelper::AssignUString(USearchHelper::uStr_SearchFunctionParamError, errorString);
				ErrorManager::PlainAlert((StringPtr) errorString);
#endif // Debug_Signal
				Throw_(paramErr);
			}
			break;
			
		case SearchError_ResultSetEmpty:
			Throw_(paramErr);
			break;

		case SearchError_ResultSetTooBig:
			Throw_(memFullErr);
			break;

		default:
			AssertFail_(false);
			Throw_(32000);	// Who knows?
			break;
			
	}
}




#pragma mark -
#pragma mark CUSTOM HEADERS
#pragma mark ÊÊÊ
////////////////////////////////////////////////////////////////////////////////////////
//
// 	Custom headers
//
//	Code to support the "Custom Header" feature.
//
//	12/1/97	
//
////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// FinishCreateAttributeMenu( MSG_SearchMenuItem* inMenuItems, UInt16& inNumItems )
//
// Finish creating the attributes menu. Add the item separator and the "EditÉ" item.

void				
CSearchManager::FinishCreateAttributeMenu( MSG_SearchMenuItem* inMenuItems, UInt16& inNumItems )
{
	if( PREF_PrefIsLocked(kCustomHeaderPref) )			// check if the preference is locked
		return;											// if it is bail out.
		
	inMenuItems[inNumItems].attrib = 0;					// add the seaparation item
	inMenuItems[inNumItems].name[0] = '-';		
	inMenuItems[inNumItems].name[1] = '\0';		
	inMenuItems[inNumItems].isEnabled = false;			// disabled
	
	CStr31	string;
	::GetIndString( string, eSTRINGS_RES_ID, eEditMenuItem ); 	// get Edit item from resource ( macfe.r)
	
	strcpy( inMenuItems[inNumItems + 1].name, string); 			// add the edit item	
	inMenuItems[inNumItems + 1].isEnabled = true;				// enabled
	inMenuItems[inNumItems + 1].attrib = eCustomizeSearchItem;
	
	inNumItems = inNumItems + 2;								// increase the menu item total					
}

////////////////////////////////////////////////////////////////////////////////////////
// GetNumberOfAttributes( MSG_ScopeAttribute& scope, void* scopeItem )
//
// Get the current number of attributes from the BE

UInt16				
CSearchManager::GetNumberOfAttributes( MSG_ScopeAttribute& scope, void* scopeItem )
{
	UInt16		numberOfAttributes;

	FailSearchError(::MSG_GetNumAttributesForSearchScopes(
		CMailNewsContext::GetMailMaster(),
		scope,
		&scopeItem,
		1,
		&numberOfAttributes));

	// check if the array is allocated
	if( mAttributeItems == nil )
		mAttributeItems = new MSG_SearchMenuItem[numberOfAttributes + deltaAttributes];
	else
	// check that is allocated and big enough
	if( numberOfAttributes > mLastNumOfAttributes )
	{
		delete mAttributeItems;
		mAttributeItems = new MSG_SearchMenuItem[numberOfAttributes + deltaAttributes];
	} 				

	// return the number of attributes
	return numberOfAttributes;
}


////////////////////////////////////////////////////////////////////////////////////////
// EditCustomHeaders()
//
// Bring up the dialog for editing the custom headers

void		
CSearchManager::EditCustomHeaders()
{
	StDialogHandler customHeadersDlgHandler( resIDT_CustomHeadersDialog, nil );
	
	LWindow*	customHeadersDlg = customHeadersDlgHandler.GetDialog();
	AssertFail_( customHeadersDlg != nil );
		
	// get the new, edit, delete buttons and table
	LGAPushButton* newHeaderBtn = dynamic_cast<LGAPushButton*>( customHeadersDlg->FindPaneByID('chNW') );
	AssertFail_( newHeaderBtn != nil ); 
	
	LGAPushButton* editHeaderBtn = dynamic_cast<LGAPushButton*>( customHeadersDlg->FindPaneByID('chED') );
	AssertFail_( editHeaderBtn != nil ); 
	
	LGAPushButton* deleteHeaderBtn = dynamic_cast<LGAPushButton*>( customHeadersDlg->FindPaneByID('chDE') );
	AssertFail_( deleteHeaderBtn != nil ); 
	
	CSingleTextColumn* customHeaderTbl = dynamic_cast<CSingleTextColumn*>( customHeadersDlg->FindPaneByID('chTL') );
	AssertFail_( customHeaderTbl != nil );
			
	// get the custom headers
	char 	*customHeaderBuffer = nil; 
	try
	{
		int error = ::PREF_CopyCharPref( kCustomHeaderPref, &customHeaderBuffer );
	}
	catch( 	mail_exception& error )
	{
		error.DisplaySimpleAlert();
		return;
	}

	// fill up the table
	STableCell cell(1, 1);
	if ((customHeaderBuffer != nil) && (*customHeaderBuffer != '\0'))	// make sure you got something to display
	{
		FillUpCustomHeaderTable( customHeaderBuffer, customHeaderTbl );
		customHeaderTbl->SelectCell( cell );
	}
	
	// free the custom header buffer
	if (customHeaderBuffer)
		XP_FREE(customHeaderBuffer);
	
	customHeaderTbl->AddListener( &customHeadersDlgHandler );
	customHeadersDlg->FocusDraw();
	
	// loop while the dialog box is up
	bool			notDone = true;	
	MessageT		theMessage;
	
	while( notDone )
	{
		// pool the message
		theMessage = customHeadersDlgHandler.DoDialog();
		switch( theMessage )
		{
			case msg_OK:
				const char *customHeadersBuffer = GetCustomHeadersFromTable( customHeaderTbl );
				::PREF_SetCharPref( kCustomHeaderPref, customHeadersBuffer );
				::PREF_SavePrefFile();
				delete  customHeadersBuffer;	// delete the buffer
				notDone = false;
				break;
		
			case msg_Cancel:
				notDone = false;
				break;
		
			case msg_NewHeader:
				Str255	newHeader;
				if( InputCustomHeader( newHeader, true) )
				{
					customHeaderTbl->InsertRows(1, 0, newHeader, (newHeader[0] + 1), true );	
					customHeaderTbl->SelectCell( STableCell(1, 1) ); 
				}
				break;
		
			case msg_EditHeader:
			case msg_SingleTextColumnDoubleClick:
				cell.SetCell(0, 0);
				Str255	chosenHeader;
				Uint32	size = kMaxStringLength;
				if( customHeaderTbl->GetNextSelectedCell( cell ))			// get selected cell
				{
					customHeaderTbl->GetCellData( cell, chosenHeader, size );  
					if( InputCustomHeader( chosenHeader, false ) )
					{
						customHeaderTbl->RemoveRows(1L, cell.row, true );
						customHeaderTbl->InsertRows(1, 0, chosenHeader, (chosenHeader[0] + 1), true );		
						customHeaderTbl->SelectCell( STableCell(1, 1) ); 
						if( !deleteHeaderBtn->IsEnabled() )
						{
							deleteHeaderBtn->EnableSelf();	
							editHeaderBtn->EnableSelf();
						}		
					}
				}
				break;
		
			case msg_DeleteHeader:
				cell.SetCell(0, 0);
				if( customHeaderTbl->GetNextSelectedCell( cell ) )		// get selected cell
				{
					customHeaderTbl->RemoveRows(1L, cell.row, true );	// remove it
					STableCell firstCell(1, 1);
					customHeaderTbl->SelectCell( firstCell );
				}				
				break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// InputCustomHeader( const StringPtr ioHeader, bool new )
//
// Edit or input new custom header

bool
CSearchManager::InputCustomHeader( const StringPtr ioHeader, bool newHeader )
{
	StDialogHandler inputHeadersDlgHandler( resIDT_CustomHeadersInputDialog, nil );

	LWindow*	inputHeadersDlg = inputHeadersDlgHandler.GetDialog();
	AssertFail_( inputHeadersDlg != nil );

	CSearchEditField* inHeaderField = dynamic_cast<CSearchEditField*>( inputHeadersDlg->FindPaneByID('chIO') );
	AssertFail_( inHeaderField != nil ); 
		
	// initialize edit field
	
	if( !newHeader )
	{
		inHeaderField->SetDescriptor( ioHeader );
		inHeaderField->SelectAll();
	}
	else
		inHeaderField->SetDescriptor( CStr255("") );	 
		
	LCommander::SwitchTarget(inHeaderField);
	inHeaderField->Refresh();

	// attach the custom header validation field
	inHeaderField->SetKeyFilter( UMailFilters::CustomHeaderFilter );
		
	MessageT		theMessage;
	bool 			changed = false;
	bool			notDone = true;	
	
	while( notDone )
	{
		// pool the message
		theMessage = inputHeadersDlgHandler.DoDialog();
		switch( theMessage )
		{
			case msg_OK:
				inHeaderField->GetDescriptor( ioHeader );
				changed = true;
				notDone = false;
				break;
		
			case msg_Cancel:
				notDone = false;
				break;
		}
	}
	return changed;
}
	
////////////////////////////////////////////////////////////////////////////////////////
// FillUpCustomHeaderTable( const char* buffer, const CSingleTextColumn* table )
//
// Fill up the table in the dialog

void
CSearchManager::FillUpCustomHeaderTable( const char* buffer, CSingleTextColumn* table )
{
	int 			i;
	int 			start = 0;
	int				stringLength;
	char			customHeaderStr[256];
	Uint32			bufferLength = strlen(buffer);
	Uint32			customHeaderLength;
	TableIndexT		row = 0;
			
	// get the individual custom headers
	for (i = 0; i <= bufferLength; i++)
	{
		if (buffer[i] == eCustomHeaderSeparator ||  buffer[i] == '\0')
		{
			// fill up a string
			stringLength = MIN(i - start, sizeof(customHeaderStr) - 1);
			strncpy(&customHeaderStr[0], &buffer[start], stringLength);
			customHeaderStr[stringLength] = '\0';
			start = i + 1;
			customHeaderLength = strlen(customHeaderStr) + 1;
			// proper insertion
			table->InsertRows(1, row++, c2pstr(customHeaderStr), customHeaderLength, false);
		}
	}
	table->Refresh();
}

////////////////////////////////////////////////////////////////////////////////////////
// GetCustomHeadersFromTable( CSingleTextColumn* table, char *buffer )
//
// Return the data from the custom header table, properly formated for the preferences
// 
// *** Be aware that you have to delete the returned pointer ***
//
const char*
CSearchManager::GetCustomHeadersFromTable( CSingleTextColumn* table )
{
	STableCell 		cell;
	Uint32 			bufferSize;
	Uint32 			customHeaderLength;
	
	// get size of custom headers
	bufferSize = 0;
	cell.SetCell(0, 0);
	while( table->GetNextCell( cell ) )
	{
		customHeaderLength = 0;
		table->GetCellData( cell, nil, customHeaderLength );
		bufferSize += customHeaderLength;	// the header is a pascal string, the space taken by the extra
											// byte at offset 0 will be used to store the string separators
	}
	
	// allocate buffer for custom headers
	char*			buffer = nil;
	char*			marker = nil;
	
	if (bufferSize == 0)
		bufferSize = 1;				// need space for '\0'
	buffer = new char[ bufferSize ];
	if( buffer == nil )				// if can't allocate throw exception
		throw bufferSize;
	marker = buffer;
	
	// separators
	char			separator = eCustomHeaderSeparator;
	Str255			temp;
	
	cell.SetCell(0, 0);		// reset the cell to beginning of the table
	while( table->GetNextCell( cell ) )
	{
		customHeaderLength = kMaxStringLength;
		table->GetCellData( cell, temp, customHeaderLength ); 	// get the cell data
		customHeaderLength = temp[0];							// find its length
		memcpy( marker, &temp[1], customHeaderLength );			// copy it to the buffer
		marker += customHeaderLength;							// increase the data marker
		*(marker ++) = eCustomHeaderSeparator;					// add the headers separator
	}
	
	// add the end of line character
	if (marker > buffer)
		marker --;		// overwrite the last separator
	*marker = '\0';
	
	return buffer;
}

////////////////////////////////////////////////////////////////////////////////////////
// GetSelectedCustomHeader( const SearchLevelInfoT* curLevel ) const
//
// Returns the string corresponding to the particular custom header menu item

char*	
CSearchManager::GetSelectedCustomHeader( const SearchLevelParamT* curLevel ) const
{
	return mAttributeItems[curLevel->customHeaderPos - 1].name;
}
