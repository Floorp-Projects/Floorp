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



// CSearchManager.h

#pragma once

/*======================================================================================
	DESCRIPTION:	Handles interaction with BE and management of search window.
 					All calls to the BE search engine bottleneak through this class.
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

// BE stuff

#include "msg_srch.h"

// Other files

//"search.cpp"

class CSearchPopupMenu;
class CSearchEditField;
class CSearchDateField;
class CSearchTabGroup;
class CStr255;

class LGAPushButton;
class LGAPopup;
class LGARadioButton;
class CSingleTextColumn;

#pragma mark -
/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/

// Information about a given search level
typedef struct {
//	CListenerCaption		*booleanOperator;	no longer used: we have a global AND/OR now  (the code was removed, it was not very clean anyway)
	CSearchPopupMenu		*attributesPopup;
	CSearchPopupMenu		*operatorsPopup;
	CSearchPopupMenu		*valuePopup;
	CSearchEditField		*valueText;
	CSearchEditField		*valueInt;
	CSearchDateField		*valueDate;
	LPane					*lastVisibleValue;
} SearchLevelInfoT;

typedef struct {
	char	text[128];
} SearchTextValueT;

typedef struct {
	MSG_SearchValue		val;
	MSG_SearchOperator	op;
	XP_Bool				boolOp;		// we have a global AND/OR now but we continue to use
									// that flag for compatibility with existing rules
	long				customHeaderPos;
} SearchLevelParamT;

#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

class CSearchManager : public LListener,
					   public LBroadcaster {
					   
public:
						CSearchManager();
	virtual				~CSearchManager();
	void				AboutToClose();
	Boolean				GetBooleanOperator() const;

						// IDs for panes in associated view, also messages that are broadcast to this object
						
						enum {
							paneID_Attributes = 'ATTR'			// CSearchPopupMenu *, 	attributes popup
						,	paneID_Operators = 'OPER'			// CSearchPopupMenu *, 	operators popup
						,	paneID_TextValue = 'TXVL'			// nil, 		text value
						,	paneID_IntValue = 'ioNT'			// nil, 		integer value
						,	paneID_PopupValue = 'POVL'			// nil, 		popup value
						,	paneID_DateValue = 'DTVL'			// nil, 		date value
						,	paneID_More = 'MORE'				// nil, 		more button
						,	paneID_Fewer = 'FEWR'				// nil, 		fewer button
						,	paneID_Clear = 'CLER'				// nil, 		clear button
						,	paneID_ParamEncl = 'PENC'			// nil, 		parameter enclosure
						,	paneID_ParamSubEncl = 'SENC'		// nil, 		subview parameter enclosure
						, 	paneID_MatchAllRadio = 'MAND'		// AND radio
						, 	paneID_MatchAnyRadio = 'MOR '		// OR radio
						};
						
						// Messages that this object broadcasts
						
						enum {
							msg_SearchParametersResized = paneID_ParamEncl	// Int16 *, 	search parameters enclosure was resized
						,	msg_SearchParametersCleared = paneID_Clear		// nil, 		search parameters were cleared
						,	msg_UserChangedSearchParameters = 'UCHG'		// nil, 		search parameters were changed, broadcast
																			//				on ANY change of the parameters by the user
						,	msg_AND_OR_Toggle = 'ANOR'
						};
					   
						// Other constants
						
						enum { 
							cMaxNumSearchLevels = 5
						,	cMaxNumSearchMenuItems = 20
						,	cScopeMailFolderNewsgroup = 20
						,	cScopeMailSelectedItems = 21
						,	cScopeNewsSelectedItems = 22
						,	cPriorityScope = 30
						,	cStatusScope = 31
						};
						
						// this constants are used for the search dialogs
						enum {
							 eAND_STR = 1, 				// location of the AND string in macfe.r
							 eOR_STR = 2,				// location of the OR string in macfe.r
							 eEditMenuItem = 3,			// location of the Customize string in macfe.r
							 eSTRINGS_RES_ID = 901,		// string resource number
							 eCustomizeSearchItem = -3	// command number for the customize menu item
						};
																	
	OSErr				InitSearchManager(LView *inContainer, CSearchTabGroup *inTabGroup,
										  MSG_ScopeAttribute inScope, LArray *inFolderScopeList = nil );

	Boolean				CanSearch() const;
	void				SetIsSearching( Boolean inValue ) { mIsSearching = inValue; }
	Boolean				IsSearching() const {
							return mIsSearching;
						}
	MSG_Pane			*GetMsgPane() const {
							return mMsgPane;
						}
	Int16				GetNumVisibleLevels() const {
							return mNumVisLevels;
						}
	Int16				GetNumDefaultLevels() const {
							return 1;
						}
	Int16				GetNumLevels() const {
							return mNumLevels;
						}
	Int32				GetNumResultRows() const {
							return (mMsgPane == nil) ? 0 : MSG_GetNumLines(mMsgPane);
						}

	void				SetSearchScope(MSG_ScopeAttribute inScope, LArray *inFolderScopeList = nil);
	void				SetNumVisibleLevels(Int32 inNumLevels);


	void				StopSearch(MWContext *inContext = nil);
	void				StartSearch()
						{
							// Don't switch target when disabling
							StValueChanger<Boolean>	change(mCanRotateTarget, false);
							mParamEnclosure->Disable();	
						}
	/* void				SaveSearchResults(); Not implemented -- 10/30/97 */

	void				ReadSavedSearchStatus(LStream *inStatusData);
	void				WriteSavedSearchStatus(LStream *outStatusData);

	void				GetSearchParameters(SearchLevelParamT *outSearchParams);
	void				SetSearchParameters(Int16 inNumVisLevels, const SearchLevelParamT *inSearchParams);
	
	// For accessing result data
	
						

//	MWContext 			*IsResultElementOpen(MSG_ViewIndex inIndex, MWContextType *outType);

	void 				PopulateAttributesMenus(MSG_ScopeAttribute inScope);
	void 				PopulatePriorityMenu(CSearchPopupMenu *inMenu);
	void 				PopulateStatusMenu(CSearchPopupMenu *inMenu);
	static void			FailSearchError(MSG_SearchError inError);
	void				SetWinCSID(int16 wincsid);
	
	////////////////////////////////////////////////////////////////////////////////////////
	// Custom headers				
	virtual char*						GetSelectedCustomHeader( const SearchLevelParamT* curLevel ) const;
	virtual UInt16						GetNumberOfAttributes( MSG_ScopeAttribute& scope, void* scopeItem );
	virtual const char*					GetCustomHeadersFromTable( CSingleTextColumn* table );
	virtual const MSG_SearchMenuItem*	GetSearchMenuItems() const { return mAttributeItems; }
	virtual const UInt16&				GetLastNumOfAttributes() const { return mLastNumOfAttributes; };

	void				AddScopesToSearch(MSG_Master *inMaster);
	void				AddParametersToSearch();
	void				SetMSGPane( MSG_Pane* inPane) { mMsgPane = inPane ;}
protected:

	void								MessageAttributes( SearchLevelInfoT& searchLevel );

	virtual void		ListenToMessage(MessageT inMessage, void *ioParam = nil);

	// Utility methods

	MSG_ScopeAttribute	GetBEScope(MSG_ScopeAttribute inScope) const;
	
	void 				PopulateOperatorsMenus(MSG_ScopeAttribute inScope);

						enum ECheckDirection { eCheckForward = 1, eCheckBackward = 2, eCheckNothing = 3 };
	void 				SelectSearchLevel(Int16 inBeginLevel, ECheckDirection inCheckDirection);
	Int32 				GetSelectedSearchLevel();

//	void				MessageAttributes(CSearchPopupMenu *inAttributesMenu);

	void				MessageOperators(CSearchPopupMenu *inOperatorsMenu);
	void				MessageSave();
	void				MessageMoreFewer(Boolean inMore);
	void				MessageClear();

	void				ClearSearchParams(Int16 inStartLevel, Int16 inEndLevel);
	
	


	//void				UpdateMsgResult(MSG_ViewIndex inIndex);
	
	void				UserChangedParameters() {
							if ( mBroadcastUserChanges ) BroadcastMessage(msg_UserChangedSearchParameters);
						}
	
	////////////////////////////////////////////////////////////////////////////////////////
	// Custom headers				
	virtual	void		EditCustomHeaders();
	virtual void		FillUpCustomHeaderTable( const char buffer[], CSingleTextColumn* table );
	virtual void		FinishCreateAttributeMenu( MSG_SearchMenuItem* inMenuItems, UInt16& inNumItems );
	virtual bool 		InputCustomHeader( const StringPtr ioHeader, bool newHeader );
	
	
	enum 	{
				eCustomHeaderSeparator = ' '
			};
			
	// Instance variables
	
	Int32				mNumLevels;			// Number of possible search levels
	Int32				mNumVisLevels;		// Number of currently visible search levels
	Int32				mLevelResizeV;		// Visible margin between levels
		
	SearchLevelInfoT	mLevelFields[cMaxNumSearchLevels];
	
	LGAPushButton		*mMoreButton;
	LGAPushButton		*mFewerButton;
	LGARadioButton		*mMatchAllRadio;
	LGARadioButton		*mMatchAnyRadio;
	LView				*mParamEnclosure;
	
	Boolean 			mCanRotateTarget;
	Boolean 			mIsSearching;
	
	// BE related
	
	MSG_SearchMenuItem	mSearchMenuItems[cMaxNumSearchMenuItems];

	// *** Attributes menu support
	MSG_SearchMenuItem 	*mAttributeItems;		// menu items array for attributes
	UInt16				mLastNumOfAttributes; 	// last number of attributes
	
	MSG_ScopeAttribute	mLastMenuItemsScope;
	MSG_SearchAttribute	mLastMenuItemsAttribute;
	UInt16				mNumLastMenuItems;

	MSG_ScopeAttribute	mCurrentScope;
	LArray				*mFolderScopeList;	// List of MSG_FolderInfo * for current scope
	
	MSG_Pane			*mMsgPane;			// Search message pane during an actual search
	
	
	
	Boolean 			mBroadcastUserChanges;
	
	Boolean				mBoolOperator;
};


// StSearchDataBlock

class StSearchDataBlock {

public:

						enum { eAllocateStrings = true, eDontAllocateStrings = false };
						
						StSearchDataBlock() :
										  mData(nil) {
						}
						StSearchDataBlock(Int16 inNumSearchLevels, Boolean inAllocateStrings = true) :
										  mData(nil) {
							Allocate(inNumSearchLevels, inAllocateStrings);
						}
						~StSearchDataBlock() {
							Dispose();
						}
						
	void				Allocate(Int16 inNumSearchLevels, Boolean inAllocateStrings = true) {
							Int32 size = inAllocateStrings ? ((sizeof(SearchLevelParamT) + sizeof(SearchTextValueT)) * inNumSearchLevels) :
										  					 (sizeof(SearchLevelParamT) * inNumSearchLevels);
							
							delete mData;
							
							mData = (SearchLevelParamT *) new char[size];
							FailNIL_(mData);
							SearchLevelParamT *data = mData;
							SearchTextValueT *text = (SearchTextValueT *) &data[inNumSearchLevels];
							for (Int16 i = 0; i < inNumSearchLevels; ++i) data[i].val.u.string = inAllocateStrings ? text[i].text : nil;
						}
	void				Dispose() {
								delete mData;
								mData = nil;
							
						}
	SearchLevelParamT	*GetData() {
							return mData;
						}
private:
	
	SearchLevelParamT	*mData;
};
