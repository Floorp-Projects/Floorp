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



// MailNewsFilters.cp

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#define DEBUGGER_ASSERTIONS

#include "MailNewsFilters.h"
#include "CSearchManager.h"
#include "SearchHelpers.h"
#include "UMailFolderMenus.h"
#include "CCAption.h"

#include "UStClasses.h"
#include "CMouseDragger.h"
#include "uprefd.h"
#include "macutil.h"
#include "MailNewsgroupWindow_Defines.h"
#include "uerrmgr.h"
#include "CMailNewsContext.h"
#include "Netscape_Constants.h"
#include "xp_str.h"
#include "UGraphicGizmos.h"
#include "LFlexTableGeometry.h"
#include "CTableKeyAttachment.h"
#include "URobustCreateWindow.h"
#include "CInlineEditField.h"

#include <URegions.h>
#include <LTableView.h>
#include <LTableMonoGeometry.h>
#include <LTableSingleSelector.h>
#include <LTableArrayStorage.h>
#include <UAttachments.h>
#include <LActiveScroller.h>
#include <LGAPushButton.h>
#include <LGARadioButton.h>
#include <LBroadcasterEditField.h>
#include <LGABox.h>
#include "UIHelper.h"
#include "CMessageFolder.h"
#include <PP_KeyCodes.h>

// BE Files

#include "msg_filt.h"

class CFiltersTableView;

// Related files

//#include "filters.cpp"
//#include "allxpstr.h"
//#include "msg.h"
//#include "msgcom.h"
//#include "msg_filt.h"
//#include "pmsgfilt.h"
//#include "pmsgsrch.h"
//#include "msgglue.cpp"
//#include "ptrarray.h"

#define TABLE_TO_FILTER_INDEX(a)	((a) - 1)
#define ABS_VAL(v)					(((v) < 0) ? -(v) : (v))

#define USE_NEW_FILTERS_SAVE


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/

static const ResIDT cEnabledCheckCursorID = 29200;
static const ResIDT cHandOpenCursorID = 29201;
static const ResIDT cHandClosedCursorID = 29202;

static const Int16 cColTextHMargin = 3;

static const Int16 cDisclosureVMargin = 4;
static const Int16 cFiltersVMargin = 21;

// Save window status version

static const UInt16 cFiltersSaveWindowStatusVersion = 	0x0008;


/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/

#pragma mark -




// CMailFolderPopup

class CMailFolderPopup : public CGAIconPopup,
						 public CGAIconPopupFolderMixin {

public:

						enum { class_ID = 'FDPU' };
						CMailFolderPopup(LStream *inStream) :
									 	 CGAIconPopup(inStream) {
						}
	virtual void		GetCurrentItemTitle(Str255 outItemTitle) {
							MGetCurrentMenuItemName(outItemTitle);
						}
	
	virtual void 		FinishCreateSelf(void) {
							CGAIconPopup::FinishCreateSelf();
							CMailFolderMixin::UpdateMailFolderMixinsNow(this);
						}
protected:

};


class CMailFiltersWindow : public CMediatedWindow,
						   public CSaveWindowStatus,
						   public LListener {
					  
public:
	
	typedef CMediatedWindow Inherited;

	// Stream creator method

						enum { class_ID = 'FilT', pane_ID = class_ID, res_ID = 8700 };

						// IDs for panes in associated view, also messages that are broadcast to this object
						
						enum {
							paneID_New = 'NEWF'						// New filter
						,	paneID_Duplicate = 'DUPL'				// Duplicate filter
						,	paneID_Table = 'Tabl'					// Filter table
						,	paneID_Description = 'DESC'				// Filter description
						,	paneID_FiltersLog = 'FLOG'				// Filter log checkbox
						,	paneID_DiscloseFilterAction = 'DFLT'	// Disclosure triangle
						,	paneID_TopFrameEnclosure = 'TOPE'		// Enclosure for top frame panes
						,	paneID_FilterActionView = 'FAVW'		// Enclosure for filter action panes
						,	paneID_Actions = 'ACTN'				// CSearchPopupMenu *, actions popup
						,	paneID_Folders = 'FLDR'				// CSearchPopupMenu *, folders popup
						,	paneID_Priority = 'PRIO'			// CSearchPopupMenu *, priority popup
						, 	paneID_MatchAllRadio = 'MAND'		// AND radio
						, 	paneID_MatchAnyRadio = 'MOR '		// OR radio
						, 	paneID_ServerSideFilterButton = 'SFBT' // Server side filters button
						,   paneID_FolderToFilterScopePopup = 'MfPM'					// folder popup
		 
						};
						
						CMailFiltersWindow(LStream *inStream) : 
							 		  	   CMediatedWindow(inStream, WindowType_MailFilters),
							 		  	   CSaveWindowStatus(this),
							 		  	   LListener(),
							 		  	   mFiltersTable(nil),
							 		  	   mFilterDescriptor(nil),
							 		  	   mDiscreteVSizing(1),
							 		  	   mDescriptionTextChangedIndex(0),
							 		  	   mFilterActionChangedIndex(0),
										   mFilterActionDisclosure(nil),
										   mFilterActionView(nil),
										   mMailFoldersPopup(nil),
										   mMailPriorityPopup(nil),
							 		  	   mSavedChanges(false),
							 		  	   mMailNewsContext(nil),
							 		  	   mRepopulateFlag( 0 ), mRuleType( filterInboxRule )
							 		  	   {
							SetPaneID(pane_ID);
							SetRefreshAllWhenResized(false);
						}
	virtual 			~CMailFiltersWindow(void);
						
	void				RecalcMinMaxStdSizes(void);

	virtual	const CSearchManager&		GetSearchManager() const { return mSearchManager; }
	virtual void						Activate();
	virtual void						Deactivate()
										{  
					    					SaveCurrentFilterModifications();
					    					Inherited::Deactivate();
				    					}
	void	LoadFolderToFilter(  );
protected:

	// Overriden PowerPlant

	virtual void		FinishCreateSelf(void);
	virtual void		DrawSelf(void);
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam = nil);
	virtual void		AttemptClose(void) {
							AttemptCloseWindow();
							CMediatedWindow::AttemptClose();
						}
	virtual void		DoClose(void) {
							AttemptCloseWindow();
							CMediatedWindow::DoClose();
						}
	virtual void		DoSetBounds(const Rect &inBounds);

	virtual void		ReadWindowStatus(LStream *inStatusData);
	virtual void		WriteWindowStatus(LStream *outStatusData);
	virtual UInt16		GetValidStatusVersion(void) const {
							return cFiltersSaveWindowStatusVersion;
						}
	
	void				UpdateToFilterLevels(Int16 inDeltaV);

	// Command and message methods

	void				FilterSelectionChanged(void);
	void				ShowHideFilterAction(void);
	Boolean				FilterActionIsVisible(void) const {
							return (mFilterActionDisclosure ? (mFilterActionDisclosure->GetValue() != 0) : false);
						}
	
	void				PopulateActionsMenu(void);
	
	void				MessageActions(CSearchPopupMenu *inActionsMenu);
	void				SaveCurrentFilterModifications(void);

	void				OpenFiltersList(void);
	void				UpdateActionToFilter(Boolean inEvenIfNotVisible = false);
	void				UpdateFilterToAction(Int32 inRow);
	char *				GetEscapedTempString(char * inStr);

	void				EnableDisableFrameBindings(Boolean inEnable, SBooleanRect *ioTopFrameBinding,
												   SBooleanRect *ioActionBinding, SBooleanRect *ioDisclosureBinding);

	virtual ResIDT		GetStatusResID(void) const { return res_ID; }
	
	
	// Instance variables ==========================================================
	UInt32				mRepopulateFlag;
	CFiltersTableView	*mFiltersTable;
	LBroadcasterEditField	*mFilterDescriptor;
	Int32				mDescriptionTextChangedIndex;
	Int32				mFilterActionChangedIndex;
	Int16				mDiscreteVSizing;		// Discrete vertical sizing for the window
	Int16				mMinActionViewPortTop;	// Minimum location for the filter action view top
	Int16				mMinVSizeNoAction;		// Minimum size for the window
	
	Boolean				mSavedChanges;

	LControl			*mDuplicateButton;
	LControl			*mFilterActionDisclosure;
	LView				*mFilterActionView;
	
	CMailFolderPopup	*mMailFoldersPopup;
	CSearchPopupMenu	*mMailPriorityPopup;
	CMailFolderPopup	*mFolderToFilerPopup;
	CSearchManager		mSearchManager;
	CMailNewsContext	*mMailNewsContext;
	LGARadioButton		*mMatchAllRadio;
	LGARadioButton		*mMatchAnyRadio;
	MSG_FilterType 		mRuleType;
};


// CFiltersTableView

class CFiltersTableView : public CStandardFlexTable, public LListener {
private:
	typedef CStandardFlexTable Inherited;
public:

						// Broadcast messages
	
						enum { 
							msg_FilterTableSelectionChanged = 'chng' 	// nil
		   				,	msg_DoubleClickCell = 'doub' 				// nil
		   				,	msg_DeleteCell = 'dele' 					// nil
						,	msg_SaveModifications = 'save' 				// nil
		   				};
	
						// IDs for panes in associated view, also messages that are broadcast to this object
						
						enum {
							paneID_OrderCaption = 'ORDR'
						,	paneID_NameCaption = 'NAME'
						,	paneID_EnabledCaption = 'ENBL'
						,	paneID_EditTitle = 'EDTL'				// Edit field for title
						};
						
						enum {	// Icon IDs
							icm8_EnabledCheck = 29201
						,	kEnabledCheckIconID = 15237
						,	kDisabledCheckIconID = 15235
						,	cSmallIconWidth = 16
						,	cSmallIconHeight = 16
						};

						enum { class_ID = 'FlTb' };
						CFiltersTableView(LStream *inStream) :
										  CStandardFlexTable(inStream),
										  mEditFilterName(nil),
										  mEditFilterNameRow(0),
										  mFilterList(nil), mRuleType( filterInboxRule ) {
							SetRefreshAllWhenResized(false);
						}
	virtual				~CFiltersTableView(void);
			
	virtual void		RemoveRows(Uint32 inHowMany, TableIndexT inFromRow, Boolean inRefresh);
	void				MoveRow(TableIndexT inCurrRow, TableIndexT inNewRow,
								Boolean inRefresh);

	virtual void		ApplyForeAndBackColors(void) const {	// Make the thing public!
							LTableView::ApplyForeAndBackColors();
						}

	void				UpdateFilterNameChanged(void);
	void				SelectEditFilterName(TableIndexT inRow);
	
 
	MSG_Filter			*GetFilterAtRow(TableIndexT inRow) {
							AssertFail_(mFilterList != nil);
							MSG_Filter *filter = nil;
							Int32 error = MSG_GetFilterAt(mFilterList, TABLE_TO_FILTER_INDEX(inRow), &filter);
							AssertFail_(error == FilterError_Success);
							AssertFail_(filter != nil);
							return filter;
						}
	void				GetFilterNameAtRow(TableIndexT inRow, CStr255 *outName) {
							char *name = nil;
							Int32 error = MSG_GetFilterName(GetFilterAtRow(inRow), &name);
							Assert_(error == FilterError_Success);
							Assert_(name != nil);
							*outName = name;
						}
	Boolean				GetFilterEnabledAtRow(TableIndexT inRow) {
							XP_Bool enabled;
							Int32 error = MSG_IsFilterEnabled(GetFilterAtRow(inRow), &enabled);
							Assert_(error == FilterError_Success);
							return enabled;
						}
	void				GetFilterDescription(TableIndexT inRow, CStr255 *outDescription) {
							char *description = nil;
							Int32 error = MSG_GetFilterDesc(GetFilterAtRow(inRow), &description);
							Assert_(error == FilterError_Success);
							Assert_(description != nil);
							*outDescription = description;
						}
	Boolean 			GetLogFiltersEnabled(void) {
							Assert_(mFilterList != nil);
							Boolean isEnabled = MSG_IsLoggingEnabled(mFilterList) ? true : false;
							return isEnabled;
						}
	void				GetFilterActionAtRow(TableIndexT inRow, StSearchDataBlock *outData, Int16 *outNumLevels,
											 MSG_RuleActionType *outAction, void **outValue);
	void				SetFilterActionAtRow(TableIndexT inRow, StSearchDataBlock *inData, 
											 Int16 inNumLevels, MSG_RuleActionType inAction, 
											 void *inValue);

	void				SetFilterList(MSG_FilterList *inFilterList) {
							mFilterList = inFilterList;
						}
	void				SetFilterNameAtRow(TableIndexT inRow, const CStr255 *inName) {
							FailFiltersError(MSG_SetFilterName(GetFilterAtRow(inRow), *inName));
						}
	void				SetFilterEnabledAtRow(TableIndexT inRow, Boolean inEnable) {
							FailFiltersError(MSG_EnableFilter(GetFilterAtRow(inRow), inEnable));
						}
	void				SetFilterDescription(Int32 inRow, const CStr255 *inDescription) {
							FailFiltersError(MSG_SetFilterDesc(GetFilterAtRow(inRow), *inDescription));
						}

	void				NewFilter(void);
	void				DeleteFilter(void);
	void				DuplicateFilter(void);
	void				InsertFilterAt(TableIndexT inTableFilterRow, MSG_Filter *inFilter);
	void				RemoveFilterAt(TableIndexT inTableFilterRow, MSG_Filter **outFilter);
	void				SetLogFilters(Boolean inDoLog);
	void				SaveChangedFilterList(Int32 inRow = 0);
	MSG_FilterList*		GetFilterList() { return mFilterList; }
	void				LoadFiltersList(MSG_FolderInfo* folder, MSG_FilterType type);
	void				SetRuleType ( MSG_FilterType inType ) { mRuleType = inType; }
protected:

						// Unimplemented string IDs
						
						enum { 
							uStr_FilterFunctionNotImplemented = 29143
						,  	uStr_FilterFunctionParamError = 29144
						,  	uStr_FilterDefaultName = 8753
						,  	uStr_FilterDefaultDescription = 8754
						};

	virtual void		FinishCreateSelf(void);
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam = nil);
	virtual void		ReconcileFrameAndImage(Boolean inRefresh);
	virtual Boolean		ObeyCommand(CommandT inCommand, void *ioParam);
	
	virtual void		AdjustCursorSelf(Point inPortPt, const EventRecord &/*inMacEvent*/);
	virtual Boolean		ClickSelect(const STableCell &inCell, const SMouseDownEvent &inMouseDown);
	virtual void 		TrackSelection( const SMouseDownEvent & /* inMouseDown */ ) {
							// do nothing
						};
	virtual void		ClickCell(const STableCell &inCell, const SMouseDownEvent &inMouseDown);
	virtual void		ScrollBits(Int32 inLeftDelta, Int32 inTopDelta) {
							USearchHelper::ScrollViewBits(this, inLeftDelta, inTopDelta);
						}
	virtual void		SelectionChanged(void) {
							BroadcastMessage(msg_FilterTableSelectionChanged);
						}
						enum EMouseTableLocation { eInNone = 0, eInEnabledCheck = 1, eInTitle = 2, eInCellContent = 3 };
	EMouseTableLocation	GetMouseTableLocation(Point inPortPt, Rect *outLocationRect = nil);

	void				GetFilterTitleRect(TableIndexT inRow, Rect *outTitleRect,
										   CStr255 *outNameString = nil);
	void				SwitchEditFilterName(TableIndexT inRow, const Rect *inNameLocalFrame,
											 CStr255 *inNameString, SMouseDownEvent *ioMouseDown);
	void				GetDisplayData(const STableCell &inCell, CStr255 *outDisplayText,
									   Int16 *outHorizJustType, ResIDT *outIconID);
	void				ToggleFilterEnabled(TableIndexT inRow, Rect *inLocalIconRect);

	virtual void		DrawCellContents(const STableCell &inCell, const Rect &inLocalRect);
	virtual void		SetUpTableHelpers(void);
	virtual void		DeleteSelection(void);

	virtual Int32		GetBENumRows(void) {
							Assert_(mFilterList != nil);
							Int32 numFilters;
							FailFiltersError(MSG_GetFilterCount(mFilterList, &numFilters));
							return numFilters;
						}
											 


	static void			FailFiltersError(MSG_FilterError inError);
	static void			BEDeleteFilter(Int32 inFilterIndex, MSG_FilterList *ioFilterList);
	static void			BEMoveFilter(Int32 inFromIndex, Int32 inToIndex, MSG_FilterList *ioFilterList);

	// Instance variables ==========================================================
	
	CInlineEditField	*mEditFilterName;
	Int32				mEditFilterNameRow;
	MSG_FilterList		*mFilterList;			// BE filter list
	MSG_FilterType 		mRuleType;
};


// CDeleteFilterAction

class CDeleteFilterAction : public LAction {

public:

						CDeleteFilterAction(CFiltersTableView *inTable, TableIndexT inTableFilterRow) :
								 			LAction(CMailFiltersWindow::res_ID, 1) {
							mTable = inTable;
							mTableFilterRow = inTableFilterRow;
							mFilter = nil;
						}

	virtual	void		Finalize(void) {
							if ( mFilter != nil ) {
								MSG_FilterError error = MSG_DestroyFilter(mFilter);
								mFilter = nil;
								Assert_(error == FilterError_Success);
							}
						}
						
	virtual	void		RedoSelf(void) {
							if ( mFilter == nil ) {
								mTable->RemoveFilterAt(mTableFilterRow, &mFilter);
							}
						}

	virtual	void		UndoSelf(void) {
							if ( mFilter != nil ) {
								mTable->InsertFilterAt(mTableFilterRow, mFilter);
								mFilter = nil;
							}
						}

protected:

	// Instance variables ==========================================================
	
	CFiltersTableView	*mTable;
	TableIndexT			mTableFilterRow;	// The row that the filter belonged to
	MSG_Filter			*mFilter;			// The filter itself
};


// CMoveFilterAction

class CMoveFilterAction : public LAction {

public:

						CMoveFilterAction(CFiltersTableView *inTable, TableIndexT inStartFilterRow,
										  TableIndexT inEndFilterRow) :
								 		  LAction(CMailFiltersWindow::res_ID, 2) {
							mTable = inTable;
							mStartFilterRow = inStartFilterRow;
							mEndFilterRow = inEndFilterRow;
						}

	virtual	void		RedoSelf(void) {
							mTable->MoveRow(mStartFilterRow, mEndFilterRow, true);
							mTable->SelectScrollCell(STableCell(mEndFilterRow, 1));
						}

	virtual	void		UndoSelf(void) {
							mTable->MoveRow(mEndFilterRow, mStartFilterRow, true);
							mTable->SelectScrollCell(STableCell(mStartFilterRow, 1));
						}

protected:

	// Instance variables ==========================================================
	
	CFiltersTableView	*mTable;
	TableIndexT			mStartFilterRow;	// Starting row, before redo
	TableIndexT			mEndFilterRow;		// Ending row, after redo
};

#if 0
// CFilterTableSelector

class CFilterTableSelector : public CRowTableSelector {

public:

						CFilterTableSelector(CFiltersTableView *inTableView) :
										 	 CRowTableSelector(inTableView, false) {
						
						}
	
protected:

	virtual UInt32		GetRowUniqueID(const TableIndexT inRow) const {
							return inRow;
						}
	virtual TableIndexT	GetUniqueIDRow(const UInt32 inID) const {
							return inID;
						}
};
#endif 

// CFilterTableKeyAttachment

class CFilterTableKeyAttachment : public CTableKeyAttachment {

public:

						CFilterTableKeyAttachment(CFiltersTableView *inTableView) :
										 	 	  CTableKeyAttachment(inTableView) {
						
						}
	
protected:

	virtual void		SelectCell(const STableCell &inCell, Boolean /*multiple*/ = false) {
							AssertFail_(mTableView != nil);
							(dynamic_cast<CFiltersTableView *>(mTableView))->SelectScrollCell(inCell);
						}
};


#pragma mark -

/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/


#pragma mark -

/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/


/*======================================================================================
	Register all classes associated with the filters window.
======================================================================================*/

void CFiltersWindowManager::RegisterFiltersClasses(void)
{
	RegisterClass_(CMailFiltersWindow);
	RegisterClass_(CFiltersTableView);
	RegisterClass_(CInlineEditField);
	RegisterClass_(CMailFolderPopup);
}


/*======================================================================================
	Show the filters dialog by bringing it to the front if it is not already. Create it
	if needed.
======================================================================================*/

void CFiltersWindowManager::ShowFiltersWindow(void) {

	// Find out if the window is already around
	CMailFiltersWindow *filtersDialog = dynamic_cast<CMailFiltersWindow *>(
			CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_MailFilters));

	if ( filtersDialog == nil ) {
		// Search dialog has not yet been created, so create it here and display it.
		filtersDialog = dynamic_cast<CMailFiltersWindow *>(
				URobustCreateWindow::CreateWindow(CMailFiltersWindow::res_ID, 
												  LCommander::GetTopCommander()));
	}
	
	// Select the window
	
	filtersDialog->Select();
}


#pragma mark -

/*======================================================================================
	Update any current status info.
======================================================================================*/

CMailFiltersWindow::~CMailFiltersWindow(void) {

	RemoveAllAttachments();
		// Kludgy, but prevents crash in LUndoer caused by view being destroyed before
		// attachments.

	Boolean canRotate = false;
	if ( IsVisible() ) {
		SaveCurrentFilterModifications();
		USearchHelper::FindWindowTabGroup(&mSubCommanders)->SetRotateWatchValue(&canRotate);
	}
	delete mFiltersTable;
	mFiltersTable = nil;
	if ( mMailNewsContext ) {
		mMailNewsContext->RemoveUser(this);
	}
}

/*======================================================================================
	Activate window
======================================================================================*/

void
CMailFiltersWindow::Activate()
{
	mSearchManager.SetSearchScope( scopeMailFolder, nil);
	Inherited::Activate();
}

/*======================================================================================
	Finish creating the filters dialog.
======================================================================================*/

void CMailFiltersWindow::FinishCreateSelf(void) {

	CMediatedWindow::FinishCreateSelf();

	// This is lame. The only reason we are creating a context is so that
	// we can have ownership of the master without it being deleted! We'll need
	// to change this later!
	mMailNewsContext = new CMailNewsContext(MWContextMailFilters);
	FailNIL_(mMailNewsContext);
	mMailNewsContext->AddUser(this);
	
	AddAttachment(new LUndoer);	

	mFiltersTable = dynamic_cast<CFiltersTableView *>(
							USearchHelper::FindViewSubpane(this, paneID_Table));
							
	mFilterDescriptor = USearchHelper::FindViewEditField(this, paneID_Description);
	AssertFail_(mFiltersTable != nil);

	FindUIItemPtr(this, paneID_Duplicate, mDuplicateButton);
	FindUIItemPtr(this, paneID_DiscloseFilterAction, mFilterActionDisclosure);
//	mDuplicateButton = USearchHelper::FindViewControl(this, paneID_Duplicate);
//	mFilterActionDisclosure = USearchHelper::FindViewControl(this, paneID_DiscloseFilterAction);
	mFilterActionView = USearchHelper::FindViewSubview(this, paneID_FilterActionView);
	mMailFoldersPopup = dynamic_cast<CMailFolderPopup *>(USearchHelper::FindViewSubpane(this, paneID_Folders));
	AssertFail_(mMailFoldersPopup != nil);
	mMailPriorityPopup = USearchHelper::FindSearchPopup(this, paneID_Priority);
	
	mFilterActionView->SetRefreshAllWhenResized(false);
	USearchHelper::FindViewSubview(this, paneID_TopFrameEnclosure)->SetRefreshAllWhenResized(false);
	
	{	// Get the discrete sizing for the window and resize it if necessary to make
		// the table size integral with the table's row height
		Rect frame;
		mFiltersTable->CalcPortFrameRect(frame);
		Int16 rowHeight = mFiltersTable->GetRowHeight(1);
		Int16 tableHeight = frame.bottom - frame.top;
		Int16 vResize = tableHeight % rowHeight;	// Make integral of the table row height
		if ( vResize != 0 ) {
			CSaveWindowStatus::GetPaneGlobalBounds(this, &frame);
			Int16 deltaV = rowHeight - vResize;
			frame.bottom += deltaV;
			mMinMaxSize.top += deltaV;
			
			CMediatedWindow::DoSetBounds(frame);	// Don't call our DoSetBounds()!
		}
		
		mFilterActionView->CalcPortFrameRect(frame);
		mMinActionViewPortTop = frame.top;
		mMinVSizeNoAction = mMinMaxSize.top;
		mDiscreteVSizing = rowHeight;
	}

	// initialize the foldertofilter popup control
	FindUIItemPtr( this, paneID_FolderToFilterScopePopup, mFolderToFilerPopup ); 
	CMailFolderMixin::FolderChoices filePopupChoices
		= static_cast<CMailFolderMixin::FolderChoices>(CMailFolderMixin::eWantNews + CMailFolderMixin::eWantDividers 
			+  CMailFolderMixin::eWantInbox + CMailFolderMixin::eWantPublicFolder);
	mFolderToFilerPopup->MSetFolderChoices( filePopupChoices );
	CMailFolderMixin::UpdateMailFolderMixinsNow( mFolderToFilerPopup );
	
	// Load selected  folder
	LoadFolderToFilter(  );
	
	// Initialize the search manager
	mSearchManager.AddListener(this);
	mSearchManager.InitSearchManager(this, USearchHelper::FindWindowTabGroup(&mSubCommanders),
									 scopeMailFolder);

	
	// Link to listeners

	mFiltersTable->AddListener(this);
	mFilterDescriptor->AddListener(this);
	USearchHelper::FindSearchPopup(this, paneID_Actions)->AddListener(this);
	FindUIItemPtr(this, paneID_MatchAllRadio, mMatchAllRadio );
	FindUIItemPtr(this, paneID_MatchAnyRadio, mMatchAnyRadio );
	
	mMailFoldersPopup->AddListener(this);
	mMailPriorityPopup->AddListener(this);

	UReanimator::LinkListenerToControls(this, this, GetStatusResID());
	
	// Call inherited method
	FinishCreateWindow();
	
}


/*======================================================================================
	Draw the window.
======================================================================================*/

void CMailFiltersWindow::DrawSelf(void) {

	Rect frame;
		
	if ( mFiltersTable->CalcPortFrameRect(frame) && ::RectInRgn(&frame, mUpdateRgnH) ) {
		{
			StExcludeVisibleRgn excludeRgn(mFiltersTable);
			CMediatedWindow::DrawSelf();
		}

		StColorPenState::Normalize();
		::EraseRect(&frame);
	} else {
		CMediatedWindow::DrawSelf();
	}
	
	USearchHelper::RemoveSizeBoxFromVisRgn(this);
}


/*======================================================================================
	React to message broadcast by the controls.
======================================================================================*/

void CMailFiltersWindow::ListenToMessage(MessageT inMessage, void *ioParam) {

	switch ( inMessage ) {
		case paneID_ServerSideFilterButton:
		{
			Assert_( mFolderToFilerPopup );
			CMessageFolder folder = mFolderToFilerPopup->MGetSelectedFolder();
			MSG_FolderInfo* folderInfo = (MSG_FolderInfo*)folder.GetFolderInfo();
			(void)MSG_GetAdminUrlForFolder(*mMailNewsContext, folderInfo, MSG_AdminServerSideFilters);
			break;
		}
				
		case paneID_FolderToFilterScopePopup:
			LoadFolderToFilter();
			break;

		case CFiltersTableView::msg_FilterTableSelectionChanged:
			FilterSelectionChanged();
			break;
			
		case paneID_New:
			mFiltersTable->NewFilter();
			Boolean actionVisible = (mFilterActionDisclosure->GetValue() != 0);
			if (!actionVisible)
			{
				mFilterActionDisclosure->SetValue(1);
				ShowHideFilterAction();
			}
			break;
			
		case paneID_Duplicate:
			mFiltersTable->DuplicateFilter();
			break;

		case paneID_FiltersLog:
			mFiltersTable->SetLogFilters((*((Int32 *) ioParam) != Button_Off));
			break;
			
		case paneID_DiscloseFilterAction:
			ShowHideFilterAction();
			break;

		case CFiltersTableView::msg_DeleteCell:
			mFiltersTable->DeleteFilter();
			break;
			
		case CFiltersTableView::msg_SaveModifications:
			SaveCurrentFilterModifications();
			break;
			
		case CSearchEditField::msg_UserChangedText:
			if ( (*((PaneIDT *) ioParam) == paneID_Description) && (mDescriptionTextChangedIndex < 0) ) {
				// Positive value indicates user changed
				mDescriptionTextChangedIndex = -mDescriptionTextChangedIndex;
			}
			break;
			
		case CSearchManager::msg_UserChangedSearchParameters:
			if ( mFilterActionChangedIndex < 0 ) {
				// Positive value indicates user changed
				mFilterActionChangedIndex = -mFilterActionChangedIndex;
			}
			break;

		case paneID_Actions:
			MessageActions((CSearchPopupMenu *) ioParam);
			if ( mFilterActionChangedIndex < 0 ) {
				// Positive value indicates user changed
				mFilterActionChangedIndex = -mFilterActionChangedIndex;
			}
			break;
			
		case paneID_Folders:
		case paneID_Priority:
			if ( mFilterActionChangedIndex < 0 ) {
				// Positive value indicates user changed
				mFilterActionChangedIndex = -mFilterActionChangedIndex;
			}
			break;

		case CSearchManager::msg_SearchParametersResized:
			UpdateToFilterLevels(*((Int16 *) ioParam));
			break;
			
		default:
			// No inherited method
			break;
	}
}


/*======================================================================================
	Adjust the window to stored preferences.
======================================================================================*/

void CMailFiltersWindow::ReadWindowStatus(LStream *inStatusData) {

	if ( inStatusData != nil ) {
	
		Rect bounds;
		*inStatusData >> bounds;
		
		Boolean actionVisible;
		*inStatusData >> actionVisible;
		
		mFilterActionDisclosure->SetValue(actionVisible ? 1 : 0);

		CSaveWindowStatus::MoveWindowTo(this, topLeft(bounds));

		TableIndexT lastSelectedRow;
		CStr255 lastSelectedName;

		*inStatusData >> lastSelectedRow;
		*inStatusData >> (StringPtr) lastSelectedName;
//		Int32 booleanPopupValue;
		
		Boolean didSelectCell = false;
		
		// Try to select the last selected filter
		if ( (lastSelectedRow != 0) && mFiltersTable->IsValidRow(lastSelectedRow) ) {
			CStr255 name;
			mFiltersTable->GetFilterNameAtRow(lastSelectedRow, &name);
			if ( ::EqualString(name, lastSelectedName, true, true) ) {
				mFiltersTable->SelectScrollCell(STableCell(lastSelectedRow, 1));
				didSelectCell = true;
			}
		}
		
		// Finally, try to display the last number of visible cells
		
		Int16 numVisibleCells;
		*inStatusData >> numVisibleCells;
		
		SDimension16 frameSize;
		mFiltersTable->GetFrameSize(frameSize);
		
		Int16 shouldBeHeight = mFiltersTable->GetRowHeight(1) * numVisibleCells;

		if (shouldBeHeight != frameSize.height) {
			CSaveWindowStatus::GetPaneGlobalBounds(this, &bounds);
			bounds.bottom += (shouldBeHeight - frameSize.height);
			CSaveWindowStatus::VerifyWindowBounds(this, &bounds);
			DoSetBounds(bounds);
		}

		mFiltersTable->ReadSavedTableStatus(inStatusData);
		
		if ( didSelectCell ) return;
	} else {
		mFilterActionDisclosure->SetValue(0);	// Hide action by default
		CSaveWindowStatus::MoveWindowToAlertPosition(this);
	}
	
	mSearchManager.SetNumVisibleLevels(1);
	RecalcMinMaxStdSizes();
	FilterSelectionChanged();
}


/*======================================================================================
	Get window stored preferences.
======================================================================================*/

void CMailFiltersWindow::WriteWindowStatus(LStream *outStatusData) {

	CSaveWindowStatus::WriteWindowStatus(outStatusData);

	TableIndexT lastSelectedRow = 0;
	CStr255 lastSelectedName = CStr255::sEmptyString;
	
	// Try to store the last selected filter
	STableCell selectedCell = mFiltersTable->GetFirstSelectedCell();
	if ( mFiltersTable->IsValidCell(selectedCell) ) {
		lastSelectedRow = selectedCell.row;
		mFiltersTable->GetFilterNameAtRow(lastSelectedRow, &lastSelectedName);
	}

	*outStatusData << (Boolean) FilterActionIsVisible();
	*outStatusData << lastSelectedRow;
	*outStatusData << (StringPtr) lastSelectedName;
	
	// Save number of visible rows in the table
	
	SDimension16 frameSize;
	mFiltersTable->GetFrameSize(frameSize);
	
	Int16 numVisibleCells = frameSize.height / mFiltersTable->GetRowHeight(1);
	*outStatusData << numVisibleCells;

	mFiltersTable->WriteSavedTableStatus(outStatusData);
}


/*======================================================================================
	Make resizing of the window integral to the row size of the filter table.
======================================================================================*/

void CMailFiltersWindow::DoSetBounds(const Rect &inBounds) {

	Rect newBounds = inBounds;
	
	USearchHelper::CalcDiscreteWindowResizing(this, &newBounds, 1, mDiscreteVSizing);
	
	CMediatedWindow::DoSetBounds(newBounds);
}


/*======================================================================================
	Recalculate the window's std sizes based on the current number of filters.
======================================================================================*/

void CMailFiltersWindow::RecalcMinMaxStdSizes(void) {

	Rect windBounds, viewBounds;
	CalcPortFrameRect(windBounds);
	mStandardSize.width = windBounds.right - windBounds.left;

	if ( FilterActionIsVisible() ) {
		mFilterActionView->CalcPortFrameRect(viewBounds);
		Assert_(viewBounds.top >= mMinActionViewPortTop);
		mMinMaxSize.top = windBounds.bottom - (viewBounds.top - mMinActionViewPortTop);
	} else {
		mMinMaxSize.top = mMinVSizeNoAction;
	}

	mFiltersTable->CalcPortFrameRect(viewBounds);
	
	TableIndexT rows, cols;
	mFiltersTable->GetTableSize(rows, cols);

	Int32 deltaVSize = (rows * (Int32) mFiltersTable->GetRowHeight(1)) - 
						(Int32) (viewBounds.bottom - viewBounds.top);
			
	Int32 newHeight = (Int32) (windBounds.bottom - windBounds.top) + deltaVSize;
	if ( newHeight > mMinMaxSize.bottom ) {
		newHeight = mMinMaxSize.bottom;
	} else if ( newHeight < mMinMaxSize.top ) {
		newHeight = mMinMaxSize.top;
	}
	mStandardSize.height = newHeight;
}


/*======================================================================================
	The selection of a filter changed in the table.
======================================================================================*/

void CMailFiltersWindow::FilterSelectionChanged(void) {

	SaveCurrentFilterModifications();

	STableCell selectedCell = mFiltersTable->GetFirstSelectedCell();
	Boolean filtersAreSelected = mFiltersTable->IsValidCell(selectedCell);

	// We don't really have a way to display JavaScript filters right now
	MSG_FilterType filterType;

	Boolean	enableDuplicateButton = filtersAreSelected;
	Boolean enableFilterDescription = filtersAreSelected;
	Boolean enableFilterActionView = filtersAreSelected;
	
	if (filtersAreSelected) {
		MSG_Filter *selectedFilter;
		selectedFilter = mFiltersTable->GetFilterAtRow(selectedCell.row);
		MSG_GetFilterType(selectedFilter, &filterType);
		if (filterType == filterInboxJavaScript || filterType == filterNewsJavaScript) {
			enableDuplicateButton = false;
			enableFilterActionView = false;
		}
	}

	USearchHelper::EnableDisablePane(mDuplicateButton, enableDuplicateButton);
	USearchHelper::EnableDisablePane(mFilterDescriptor->GetSuperView(), enableFilterDescription );
	USearchHelper::EnableDisablePane(mFilterActionView, enableFilterActionView);

	Boolean enableAndOrRadios = enableFilterActionView && (mSearchManager.GetNumVisibleLevels() > 1);
	USearchHelper::EnableDisablePane(mMatchAllRadio, enableAndOrRadios);
	USearchHelper::EnableDisablePane(mMatchAnyRadio, enableAndOrRadios);
	// If the filter text changed, update the BE
	
	CStr255 filterDescriptor;

	// Update the displayed text
	if ( filtersAreSelected ) {
		mFiltersTable->GetFilterDescription(selectedCell.row, &filterDescriptor);
		mDescriptionTextChangedIndex = -selectedCell.row;
	} else {
		filterDescriptor[0] = 0;
	}
	
	mFilterDescriptor->SetDescriptor(filterDescriptor);
	
	UpdateActionToFilter();
		
	if ( IsVisible() ) UpdatePort();	// For immediate action
}


/*======================================================================================
	Setup the dialog so that the specified number of search levels are visible.
======================================================================================*/

void CMailFiltersWindow::UpdateToFilterLevels(Int16 inDeltaV) {

	if ( (inDeltaV == 0) ) return;

	// Disable bottom binding of necessary panes
	
	SBooleanRect topFrameBinding, actionBinding, disclosureBinding;
	EnableDisableFrameBindings(false, &topFrameBinding, &actionBinding, &disclosureBinding);

	// Resize window
	
	mFilterActionView->ResizeFrameBy(0, inDeltaV, true);
	
	if ( FilterActionIsVisible() ) {
		Rect bounds;
		CSaveWindowStatus::GetPaneGlobalBounds(this, &bounds);
		bounds.bottom += inDeltaV;
		CMediatedWindow::DoSetBounds(bounds);	// Update window size
		RecalcMinMaxStdSizes();
	}
	
	// Reenable bottom binding of top panes
	
	EnableDisableFrameBindings(true, &topFrameBinding, &actionBinding, &disclosureBinding);
}


/*======================================================================================
	Show or hide the filter action depending upon its current state.
======================================================================================*/

void CMailFiltersWindow::ShowHideFilterAction(void) {

	Boolean actionVisible = (mFilterActionDisclosure->GetValue() != 0);
	LView *paramEnclosure = USearchHelper::FindViewSubview(mFilterActionView, CSearchManager::paneID_ParamEncl);
	
	if ( actionVisible ) {
		StValueChanger<Int32> change(*((Int32 *) &mFilterActionDisclosure), nil);
		UpdateActionToFilter(true);
	}

	Rect windowBounds, disclosureRect;
	CSaveWindowStatus::GetPaneGlobalBounds(this, &windowBounds);
	CSaveWindowStatus::GetPaneGlobalBounds(mFilterActionDisclosure, &disclosureRect);
	
	// Disable bottom binding of necessary panes
	
	SBooleanRect topFrameBinding, actionBinding, disclosureBinding;
	EnableDisableFrameBindings(false, &topFrameBinding, &actionBinding, &disclosureBinding);
	
	// Resize window
	
	if ( actionVisible ) {
		Rect enclosureBounds;
		CSaveWindowStatus::GetPaneGlobalBounds(mFilterActionView, &enclosureBounds);
		Int16 newBottom = enclosureBounds.bottom /*+ cFiltersVMargin*/;
		windowBounds.bottom = newBottom;
		CMediatedWindow::DoSetBounds(windowBounds);
		paramEnclosure->Show();
		((LGABox *) mFilterActionView)->SetBorderStyle(borderStyleGA_BezelBorder);
	} else {
		Int16 newBottom = disclosureRect.bottom + cDisclosureVMargin;
		windowBounds.bottom = newBottom;
		CMediatedWindow::DoSetBounds(windowBounds);
		paramEnclosure->Hide();
		((LGABox *) mFilterActionView)->SetBorderStyle(borderStyleGA_NoBorder);
	}
	
	// Reenable bottom binding of top panes
	
	EnableDisableFrameBindings(true, &topFrameBinding, &actionBinding, &disclosureBinding);

	RecalcMinMaxStdSizes();
}

//-----------------------------------
void CMailFiltersWindow::UpdateActionToFilter(Boolean inEvenIfNotVisible)
//-----------------------------------
{

	mFilterActionChangedIndex = 0;
	
	if ( !FilterActionIsVisible() && !inEvenIfNotVisible ) return;

	STableCell selectedCell = mFiltersTable->GetFirstSelectedCell();
	if ( !mFiltersTable->IsValidCell(selectedCell) ) return;
	
	// don't try and do anything with JavaScript filters because we can't
	MSG_Filter *selectedFilter;
	selectedFilter = mFiltersTable->GetFilterAtRow(selectedCell.row);
	MSG_FilterType filterType;
	MSG_GetFilterType(selectedFilter, &filterType);
	if (filterType == filterInboxJavaScript || filterType == filterNewsJavaScript) {
		mSearchManager.SetNumVisibleLevels(1);
		LEditField *txtField = dynamic_cast<LEditField *>(FindPaneByID(CSearchManager::paneID_TextValue));
		if (txtField != NULL)
			txtField->SetDescriptor("\p");

		return;
	}

	Try_ {
	
		StSearchDataBlock paramData;
		Int16 numLevels;
		MSG_RuleActionType action;
		void *value;

		mFiltersTable->GetFilterActionAtRow(selectedCell.row, &paramData, &numLevels, &action, &value);

		// Update to filter levels
		mSearchManager.SetSearchParameters(numLevels, paramData.GetData());

		// Update to filter action
		USearchHelper::FindSearchPopup(this, paneID_Actions)->SetCurrentItemByCommand(action);
		
		SearchLevelParamT* currentData = paramData.GetData();
		(currentData->boolOp ? mMatchAllRadio : mMatchAnyRadio)->SetValue(Button_On);

		mDescriptionTextChangedIndex = selectedCell.row;

		switch ( action )
		{
			case acMoveToFolder:
					// Set the folder name
					value = GetEscapedTempString((char*)value);
					if ( !mMailFoldersPopup->MSetSelectedFolderName((const char*) value) )
						mFilterActionChangedIndex = selectedCell.row;
				break;	
			case acChangePriority:
					// Get the priority
					AssertFail_(mMailPriorityPopup->IsLatentVisible());
					mMailPriorityPopup->SetCurrentItemByCommand(*((MSG_PRIORITY *) &value));
				break;
			case acDelete:
			case acMarkRead:
			case acKillThread:
			case acWatchThread:
				break;
			default:
				AssertFail_(false);	// Should never get here!
				break;
		}
	}
	Catch_(inErr)
	{
		USearchHelper::GenericExceptionAlert(inErr);	
		// Don't throw here! Should really never get an error!
	}
	EndCatch_

	if ( mFilterActionChangedIndex != 0 || mDescriptionTextChangedIndex != 0) SaveCurrentFilterModifications();
	mFilterActionChangedIndex = -selectedCell.row;
	mDescriptionTextChangedIndex = -selectedCell.row;
}

//-----------------------------------
void CMailFiltersWindow::UpdateFilterToAction(Int32 inRow)
//-----------------------------------
{
	// don't try and do anything with JavaScript filters because we can't
	MSG_Filter *selectedFilter;
	selectedFilter = mFiltersTable->GetFilterAtRow(inRow);
	MSG_FilterType filterType;
	MSG_GetFilterType(selectedFilter, &filterType);
	if (filterType == filterInboxJavaScript || filterType == filterNewsJavaScript)
		return;	

	Try_ {
		// Get rules
		
		Int16 numVisLevels = mSearchManager.GetNumVisibleLevels();
		AssertFail_(numVisLevels > 0);	// Make sure we get something back!

		StSearchDataBlock ruleData(numVisLevels, StSearchDataBlock::eAllocateStrings);
		mSearchManager.GetSearchParameters(ruleData.GetData());

		MSG_RuleActionType action = (MSG_RuleActionType)
				USearchHelper::FindSearchPopup(this, paneID_Actions)->GetCurrentItemCommandNum();
				
		void *value = nil;
		MSG_PRIORITY priority;
		switch ( action )
		{
			case acMoveToFolder:
					// Get the folder name
					value = (void *) mMailFoldersPopup->MGetSelectedFolderName();
					AssertFail_(value != nil);
					value = GetEscapedTempString((char*)value);
				break;
		
			case acChangePriority:
					// Get the priority
					Assert_(mMailPriorityPopup->IsLatentVisible());
					priority = (MSG_PRIORITY) mMailPriorityPopup->GetCurrentItemCommandNum();
					value = (void *) priority;
				break;

			case acDelete:
			case acMarkRead:
			case acKillThread:
			case acWatchThread:
				break;
				
			default:
				AssertFail_(false);	// Should never get here!
				break;
		}
		
		mFiltersTable->SetFilterActionAtRow(inRow, &ruleData, numVisLevels, action, value);
		
	}
	Catch_(inErr)
	{
		USearchHelper::GenericExceptionAlert(inErr);
		// Don't throw here! Should really never get an error!
	}
	EndCatch_
}

//-----------------------------------
char * CMailFiltersWindow::GetEscapedTempString(char * inStr)
//-----------------------------------
{
	// Make sure that the name is fully escaped
	static char escapedName[256];
	XP_STRCPY(escapedName, inStr);
	NET_UnEscape(escapedName);
	char * temp = NET_Escape(escapedName, URL_PATH);
	if (temp)
	{
		XP_STRCPY(escapedName, temp);
		XP_FREE(temp);
	}
	return escapedName;
}


/*======================================================================================
	The specified filter has changed. Save the filter list to disk. If inRow is 0,
	the entire list has changed, so save it all.
======================================================================================*/

void CMailFiltersWindow::SaveCurrentFilterModifications(void) {

	if ( ((mDescriptionTextChangedIndex > 0) || (mFilterActionChangedIndex > 0)) ) {

		Try_ {
			Int16 saveChangeIndex = 0;
			
			if ( (mDescriptionTextChangedIndex > 0) ) {
				CStr255 filterDescriptor;
				mFilterDescriptor->GetDescriptor(filterDescriptor);
				mFiltersTable->SetFilterDescription(mDescriptionTextChangedIndex, &filterDescriptor);
				saveChangeIndex = mDescriptionTextChangedIndex;
			}
			
			if ( (mFilterActionChangedIndex > 0) ) {
				UpdateFilterToAction(mFilterActionChangedIndex);
				Assert_(saveChangeIndex ? (saveChangeIndex == mFilterActionChangedIndex) : true);
				saveChangeIndex = mFilterActionChangedIndex;
			}
			
			mFiltersTable->SaveChangedFilterList(saveChangeIndex);
		}
		Catch_(inErr) {
			// Do nothing, we should never really get an error here
			USearchHelper::GenericExceptionAlert(inErr);
		}
		EndCatch_
		
		mDescriptionTextChangedIndex = 0;
		mFilterActionChangedIndex = 0;
	}
}


/*======================================================================================
	Enable or disable frame bindings in preparation for changing the window size.
======================================================================================*/

void CMailFiltersWindow::EnableDisableFrameBindings(Boolean inEnable, SBooleanRect *ioTopFrameBinding,
												    SBooleanRect *ioActionBinding, SBooleanRect *ioDisclosureBinding) {

	LView *topFrameEnclosure = USearchHelper::FindViewSubview(this, paneID_TopFrameEnclosure);
	LView *disclosureSuper = mFilterActionDisclosure->GetSuperView();

	if ( !inEnable ) {
		topFrameEnclosure->GetFrameBinding(*ioTopFrameBinding);
		mFilterActionView->GetFrameBinding(*ioActionBinding);
		disclosureSuper->GetFrameBinding(*ioDisclosureBinding);
		
		ioTopFrameBinding->bottom = ioActionBinding->bottom = ioDisclosureBinding->bottom = false;
	} else {
		ioTopFrameBinding->bottom = ioActionBinding->bottom = ioDisclosureBinding->bottom = true;
	}

	topFrameEnclosure->SetFrameBinding(*ioTopFrameBinding);
	mFilterActionView->SetFrameBinding(*ioActionBinding);
	disclosureSuper->SetFrameBinding(*ioDisclosureBinding);
}


/*======================================================================================
	Populate the actions menu.
======================================================================================*/

static const Int16 cMaxNumActionItems = 8;

void CMailFiltersWindow::PopulateActionsMenu(void) {
									 
	CSearchPopupMenu *theMenu = USearchHelper::FindSearchPopup(this, paneID_Actions);
	
	Assert_(theMenu->GetNumCommands() == 6);
	theMenu->SetIndexCommand(1, acMoveToFolder);
	theMenu->SetIndexCommand(2, acChangePriority);
	theMenu->SetIndexCommand(3, acDelete);
	theMenu->SetIndexCommand(4, acMarkRead);
	theMenu->SetIndexCommand(5, acKillThread);
	theMenu->SetIndexCommand(6, acWatchThread);
	
#if 0	// BE strings are lame!
	theMenu->ClearMenu();
	
	StPointerBlock menuData(sizeof(MSG_RuleMenuItem) * cMaxNumActionItems);
	
	MSG_RuleMenuItem *menuItems = (MSG_RuleMenuItem *) menuData.mPtr;
	UInt16 numMenuItems = cMaxNumActionItems;
	
	FailFiltersError(MSG_GetRuleActionMenuItems(mRuleType, menuItems, 
																	&numMenuItems));
	AssertFail_(numMenuItems > 0);
	
	MSG_RuleMenuItem *endItem = menuItems + numMenuItems;
	
	do {
		theMenu->AppendMenuItemCommand(menuItems->attrib, C2PStr(menuItems->name), true);
	} while ( ++menuItems < endItem );
#endif // 0
}


/*======================================================================================
	React to actions message.
======================================================================================*/

void CMailFiltersWindow::MessageActions(CSearchPopupMenu *inActionsMenu) {

	MSG_RuleActionType action = (MSG_RuleActionType) inActionsMenu->GetCurrentItemCommandNum();
	
	Boolean showPriorities = false, showFolders = false;
	
	if ( action == acMoveToFolder ) {
		showFolders = true;
	} else if ( action == acChangePriority ) {
		showPriorities = true;
	}
	
	USearchHelper::ShowHidePane(mMailPriorityPopup, showPriorities);
	USearchHelper::ShowHidePane(mMailFoldersPopup, showFolders);
}

void	CMailFiltersWindow::LoadFolderToFilter(   )
{
	Assert_( mFolderToFilerPopup );
	CMessageFolder folder = mFolderToFilerPopup->MGetSelectedFolder();
	MSG_FolderInfo* folderInfo = (MSG_FolderInfo*)folder.GetFolderInfo();
	MSG_FilterType type = filterInbox;
	mRuleType = filterInboxRule;
	if ( folder.IsNewsgroup() )
	{
		type = filterNews;
		mRuleType = filterNewsRule;
	}
	
	mFiltersTable->SetRuleType( mRuleType );
	PopulateActionsMenu();
	mSearchManager.PopulatePriorityMenu(mMailPriorityPopup);
	
	mFiltersTable->LoadFiltersList( folderInfo, type );
	
	LGACheckbox		*logBox;
	FindUIItemPtr(this, paneID_FiltersLog, logBox);
//	USearchHelper::FindViewControl(this, paneID_FiltersLog)
	logBox->SetValue( mFiltersTable->GetLogFiltersEnabled() ? 1 : 0);
	// Does the server support server side filters?
	XP_Bool enable = false;
	
	enable = MSG_HaveAdminUrlForFolder( folderInfo, MSG_AdminServerSideFilters );
	LControl* serverFilterButton = dynamic_cast<LControl* >(FindPaneByID( CMailFiltersWindow::paneID_ServerSideFilterButton ));
	if( serverFilterButton)
		USearchHelper::EnableDisablePane( serverFilterButton ,enable, true );
}


#pragma mark -
/*======================================================================================
	Update any current status info.
======================================================================================*/

CFiltersTableView::~CFiltersTableView(void) {

	if ( IsVisible() ) {
		UpdateFilterNameChanged();
	}
	if ( mFilterList != nil ) {
		MSG_CancelFilterList(mFilterList);
		mFilterList = nil;
	}
}


/*======================================================================================
	Finish initializing the object.
======================================================================================*/

void CFiltersTableView::FinishCreateSelf(void) {

//	long isDoubleByte;
	
	CStandardFlexTable::FinishCreateSelf();
	
	mEditFilterName = (CInlineEditField *) USearchHelper::FindViewEditField(this, paneID_EditTitle);
#if 0
	// See CInlineEditField.cp for explanation of double byte wierdness
	isDoubleByte = ::GetScriptManagerVariable(smDoubleByte);
	mEditFilterName->SetGrowableBorder(!isDoubleByte);
#endif
	mEditFilterName->AddListener(this);
	
}


/*======================================================================================
	Obey commands.
======================================================================================*/

Boolean CFiltersTableView::ObeyCommand(CommandT inCommand, void *ioParam) {

	Boolean cmdHandled = true;

	switch (inCommand) {
		
		case msg_TabSelect:
			break;

		default:
			cmdHandled = CStandardFlexTable::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return cmdHandled;
}


/*======================================================================================
	React to message broadcast by the controls.
======================================================================================*/

void CFiltersTableView::ListenToMessage(MessageT inMessage, void *ioParam) {

	switch ( inMessage ) {
	
		case CInlineEditField::msg_InlineEditFieldChanged:
			if ( (*((PaneIDT *) ioParam) == paneID_EditTitle) && (mEditFilterNameRow < 0) ) {
				mEditFilterNameRow = -mEditFilterNameRow;	// Text has changed
			}
			break;
			
		case CInlineEditField::msg_HidingInlineEditField:
			if ( (*((PaneIDT *) ioParam) == paneID_EditTitle) ) {
				UpdateFilterNameChanged();
				mEditFilterNameRow = 0;
			}
			break;

		default:
			//Inherited::ListenToMessage(inMessage, ioParam);
			break;
	}
}


/*======================================================================================
	Load the filters list and initialize the table accordingly.
======================================================================================*/

void CFiltersTableView::LoadFiltersList(MSG_FolderInfo* folder, MSG_FilterType type  )
{
	Assert_( folder );
	if( folder == NULL )
		return;
	// If there is a filterlist save it before loading in the new list
	if ( mFilterList )
		SaveChangedFilterList();
	// Open the filters list
	//FailFiltersError(MSG_OpenFilterList(CMailNewsContext::GetMailMaster(), filterInbox, 
	//									&mFilterList));
	

	FailFiltersError( MSG_OpenFolderFilterListFromMaster(CMailNewsContext::GetMailMaster(), folder, type, &mFilterList) );
		
	// Initialize the filters table with the new data
	Int32 numFilters = GetBENumRows();
	RemoveAllRows(true);
	Assert_(mRows == 0);
	InsertRows(numFilters, 0, nil, 0, true);
	
}


/*======================================================================================
	The specified filter has changed. Save the filter list to disk. If inRow is 0,
	the entire list has changed, so save it all.
======================================================================================*/

void CFiltersTableView::SaveChangedFilterList(Int32 /*inRow*/) {

	// This is a temp fix waiting for a solution!
	
	Try_ {
		AssertFail_(mFilterList != nil);
		
#ifdef USE_NEW_FILTERS_SAVE
		FailFiltersError(MSG_SaveFilterList(mFilterList));
#else // USE_NEW_FILTERS_SAVE
		// Close filter list to save it
		FailFiltersError(MSG_CloseFilterList(mFilterList));
		mFilterList = nil;
		// Open the filters list again
		FailFiltersError(MSG_OpenFilterList(CMailNewsContext::GetMailMaster(), filterInbox, 
											&mFilterList));
#endif // USE_NEW_FILTERS_SAVE
		
	}
	Catch_(inErr) {
		// This is obviously a major error! Just close and delete the window in this case
		USearchHelper::GenericExceptionAlert(inErr);
	}
	EndCatch_
	
	if ( mFilterList == nil ) {
		USearchHelper::GetPaneWindow(this)->DoClose();	// Bad, bad, bad boy! Will change later
	}
}


/*======================================================================================
	Get the filter action data for the specified row. Caller must delete memory using
	::DisposePtr().
======================================================================================*/

void CFiltersTableView::GetFilterActionAtRow(
	TableIndexT inRow, StSearchDataBlock *outData, Int16 *outNumLevels,
	MSG_RuleActionType *outAction, void **outValue)
{
	MSG_Filter *filter = GetFilterAtRow(inRow);
	
	MSG_Rule *rule = nil;
	FailFiltersError(MSG_GetFilterRule(filter, &rule));
	AssertFail_(rule != nil);
	
	{
		Int32 tempNumLevels;
		FailFiltersError(MSG_RuleGetNumTerms(rule, &tempNumLevels));
		*outNumLevels = tempNumLevels;
	}
	AssertFail_(*outNumLevels > 0);
	
	outData->Allocate(*outNumLevels, StSearchDataBlock::eDontAllocateStrings);
	SearchLevelParamT *curLevel = outData->GetData();
	Int16 index = 0;
	
	do {
		FailFiltersError(MSG_RuleGetTerm(rule, index, &curLevel->val.attribute,
										 &curLevel->op, &curLevel->val, &curLevel->boolOp, nil ));
		++curLevel;
	} while ( ++index < *outNumLevels );
			
	FailFiltersError(MSG_RuleGetAction(rule, outAction, outValue));
}


/*======================================================================================
	Update the filter to the user specified action.
======================================================================================*/

void CFiltersTableView::SetFilterActionAtRow(TableIndexT inRow, StSearchDataBlock *inData, 
											 Int16 inNumLevels, MSG_RuleActionType inAction, 
											 void *inValue) {

	MSG_Filter *newFilter = nil;
	
	Try_ {

		// Contruct a new filter (this is the only way! :-( )

		MSG_Filter *filter = GetFilterAtRow(inRow);
		
		char *text;
		FailFiltersError(MSG_GetFilterName(filter, &text));

		FailFiltersError(MSG_CreateFilter(mRuleType, text, &newFilter));
		
		FailFiltersError(MSG_GetFilterDesc(filter, &text));
		FailFiltersError(MSG_SetFilterDesc(newFilter, text));
			
		XP_Bool isEnabled;
		FailFiltersError(MSG_IsFilterEnabled(filter, &isEnabled));
		FailFiltersError(MSG_EnableFilter(newFilter, isEnabled));
		
		MSG_Rule *newRule;
		FailFiltersError(MSG_GetFilterRule(newFilter, &newRule));
		
		// Add rules to filter
		
		SearchLevelParamT *curLevel = inData->GetData(), *endLevel = curLevel + inNumLevels;

		do {
			char	*customHeader = nil;
			 
			if( curLevel->val.attribute == attribOtherHeader )
			{
				CMailFiltersWindow *filterWindow = dynamic_cast<CMailFiltersWindow *>( USearchHelper::GetPaneWindow(this) );
				customHeader = ( filterWindow->GetSearchManager() ).GetSelectedCustomHeader( curLevel );
			}
			
			FailFiltersError(MSG_RuleAddTerm(newRule, curLevel->val.attribute, 
											 curLevel->op, &curLevel->val, curLevel->boolOp, customHeader ));
		} while ( ++curLevel < endLevel );

		// Add filter action
		
		FailFiltersError(MSG_RuleSetAction(newRule, inAction, inValue));

		Int32 filterIndex = TABLE_TO_FILTER_INDEX(inRow);

		Int32 numFilters = GetBENumRows();

		// Add the new filter to the end of the list (API doesn't let us do it any other way!)
		FailFiltersError(MSG_SetFilterAt(mFilterList, numFilters, newFilter));
		newFilter = nil;

		// Remove and delete the old edit filter from the list
		BEDeleteFilter(filterIndex, mFilterList);
		--numFilters;
		
		// Move new filter into the correct position
		if ( filterIndex != numFilters ) {
			BEMoveFilter(numFilters, filterIndex, mFilterList);
		}
	}
	Catch_(inErr) {
		if ( newFilter != nil ) {
			MSG_FilterError error = MSG_DestroyFilter(newFilter);
			newFilter = nil;
			Assert_(error == FilterError_Success);
		}
	
		Throw_(inErr);
	}
	EndCatch_
}


/*======================================================================================
	Create a new filter by bringing up a dialog for user interaction.
======================================================================================*/

void CFiltersTableView::NewFilter(void) {

	STableCell selectedCell = GetFirstSelectedCell();
	if ( !IsValidCell(selectedCell) ) {
		// Add new filter to end of table if no cells selected
		GetTableSize(selectedCell.row, selectedCell.col);
	}
	++selectedCell.row;
	
	// Create a new default filter
	
	MSG_Filter *newFilter = nil;
	PostAction(nil);

	Try_ {
		AssertFail_(mFilterList != nil);

		Int32 numFilters = GetBENumRows();

		// Contruct a new filter
	
		CStr255 macString;
		USearchHelper::AssignUString(uStr_FilterDefaultName, macString);
		FailFiltersError(MSG_CreateFilter( mRuleType, macString, &newFilter));
		
		USearchHelper::AssignUString(uStr_FilterDefaultDescription, macString);
		FailFiltersError(MSG_SetFilterDesc(newFilter, macString));
		
		XP_Bool isEnabled = 1;
		FailFiltersError(MSG_EnableFilter(newFilter, isEnabled));
		
		MSG_Rule *rule = nil;
		FailFiltersError(MSG_GetFilterRule(newFilter, &rule));
		AssertFail_(rule != nil);
		
		USearchHelper::AssignUString(uStr_FilterDefaultName, macString);
		MSG_SearchValue value;
		value.attribute = attribSender;
		value.u.string = "";
		XP_Bool	boolOp = true;
		FailFiltersError(MSG_RuleAddTerm(rule, attribSender, opContains, &value, boolOp, nil ));

		// Add filter action
		
		macString[0] = 0;
		FailFiltersError(MSG_RuleSetAction(rule, acMoveToFolder, (char*)macString));
		
		// Add the new filter to the end of the list (API doesn't let us do it any other way!)
		FailFiltersError(MSG_SetFilterAt(mFilterList, numFilters, newFilter));
		newFilter = nil;

		// Move new filter into the correct position
		if ( TABLE_TO_FILTER_INDEX(selectedCell.row) != numFilters ) {
			BEMoveFilter(numFilters, TABLE_TO_FILTER_INDEX(selectedCell.row), mFilterList);
		}

		SaveChangedFilterList(selectedCell.row);

		InsertRows(1, selectedCell.row - 1, nil, 0, true);
		ScrollCellIntoFrame(selectedCell);
		SelectEditFilterName(selectedCell.row);
		((CMailFiltersWindow *) USearchHelper::GetPaneWindow(this))->RecalcMinMaxStdSizes();
	}
	Catch_(inErr) {
		if ( newFilter != nil ) {
			MSG_FilterError error = MSG_DestroyFilter(newFilter);
			newFilter = nil;
			Assert_(error == FilterError_Success);
		}
		Refresh();
		Throw_(inErr);
	}
	EndCatch_
}


/*======================================================================================
	Duplicate selected filters.
======================================================================================*/

void CFiltersTableView::DuplicateFilter(void) {
	SaveChangedFilterList();
	STableCell selectedCell = GetFirstSelectedCell();
	AssertFail_(IsValidCell(selectedCell));
	
	AssertFail_(mFilterList != nil);
		
	MSG_Filter *newFilter = nil;
	PostAction(nil);
	
	Try_ {
	
		Int32 newIndex = TABLE_TO_FILTER_INDEX(selectedCell.row);
		
		MSG_Filter *selectedFilter;
		FailFiltersError(MSG_GetFilterAt(mFilterList, newIndex, &selectedFilter));
		AssertFail_(selectedFilter != nil);
		
		char *text;
		FailFiltersError(MSG_GetFilterName(selectedFilter, &text));
		MSG_FilterType filterType;
		FailFiltersError(MSG_GetFilterType(selectedFilter, &filterType));
		
		// Contruct a new filter
		FailFiltersError(MSG_CreateFilter(filterType, text, &newFilter));

		FailFiltersError(MSG_GetFilterDesc(selectedFilter, &text));
		FailFiltersError(MSG_SetFilterDesc(newFilter, text));
			
		XP_Bool isEnabled;
		FailFiltersError(MSG_IsFilterEnabled(selectedFilter, &isEnabled));
		FailFiltersError(MSG_EnableFilter(newFilter, isEnabled));
		
		MSG_Rule *selectedRule, *newRule;
		FailFiltersError(MSG_GetFilterRule(selectedFilter, &selectedRule));
		FailFiltersError(MSG_GetFilterRule(newFilter, &newRule));
		
		Int32 numTerms;
		FailFiltersError(MSG_RuleGetNumTerms(selectedRule, &numTerms));
		
		{
			MSG_SearchAttribute attrib;
			MSG_SearchOperator op;
			MSG_SearchValue value;
			XP_Bool			boolOp;
			SearchTextValueT searchText;
			value.u.string = searchText.text;
			
			for (Int32 curTerm = 0; curTerm < numTerms; ++curTerm) {
				
				FailFiltersError(MSG_RuleGetTerm(selectedRule, curTerm, &attrib,
																&op, &value, &boolOp, nil ));
				FailFiltersError(MSG_RuleAddTerm(newRule, attrib, op, &value, boolOp, nil ));
			}
		}
		
		MSG_RuleActionType actionType;
		void *actionValue;
		FailFiltersError(MSG_RuleGetAction(selectedRule, &actionType, &actionValue));
		FailFiltersError(MSG_RuleSetAction(newRule, actionType, actionValue));

		Int32 numFilters = GetBENumRows();

		// Add the new filter to the end of the list (API doesn't let us do it any other way!)
		FailFiltersError(MSG_SetFilterAt(mFilterList, numFilters, newFilter));
		newFilter = nil;

		// Move new filter into the correct position
		++newIndex;
		if ( newIndex != numFilters ) BEMoveFilter(numFilters, newIndex, mFilterList);

		SaveChangedFilterList(++selectedCell.row);
		InsertRows(1, selectedCell.row - 1, nil, 0, true);
		ScrollCellIntoFrame(selectedCell);
		SelectEditFilterName(selectedCell.row);
		((CMailFiltersWindow *) USearchHelper::GetPaneWindow(this))->RecalcMinMaxStdSizes();
	}
	Catch_(inErr) {
		if ( newFilter != nil ) {
			MSG_FilterError error = MSG_DestroyFilter(newFilter);
			newFilter = nil;
			Assert_(error == FilterError_Success);
		}
		Refresh();
		Throw_(inErr);
	}
	EndCatch_
}


/*======================================================================================
	Delete selected filters.
======================================================================================*/

void CFiltersTableView::DeleteFilter(void) {

	STableCell selectedCell = GetFirstSelectedCell();
//	AssertFail_(IsValidCell(selectedCell));			// don't assert: list can be empty
	if (IsValidCell(selectedCell))
	{
		CDeleteFilterAction *action = new CDeleteFilterAction(this, selectedCell.row);
		FailNIL_(action);
			
		// Update our table
		if ( (selectedCell.row == mRows) && (selectedCell.row > 1) ) {
			--selectedCell.row;	// Move up one cell
		}
		Boolean validNewCell = selectedCell.row != 0;
		{
			// Turn off listening if we have a valid cell so that we are not
			// updated during an empty table selection
			StValueChanger<Boolean> change(mIsListening, !validNewCell);
			PostAction(action);
		}
		if ( validNewCell ) SelectScrollCell(selectedCell);
		((CMailFiltersWindow *) USearchHelper::GetPaneWindow(this))->RecalcMinMaxStdSizes();
	}
}


/*======================================================================================
	Insert the specified filter into the table.
======================================================================================*/

void CFiltersTableView::InsertFilterAt(TableIndexT inTableFilterRow, MSG_Filter *inFilter) {

	Int32 numFilters = GetBENumRows();

	// Add the new filter to the end of the list (API doesn't let us do it any other way!)
	FailFiltersError(MSG_SetFilterAt(mFilterList, numFilters, inFilter));

	// Move new filter into the correct position
	if ( TABLE_TO_FILTER_INDEX(inTableFilterRow) != numFilters ) {
		BEMoveFilter(numFilters, TABLE_TO_FILTER_INDEX(inTableFilterRow), mFilterList);
	}
	SaveChangedFilterList(inTableFilterRow);

	InsertRows(1, inTableFilterRow - 1, nil, 0, true);
	SelectScrollCell(STableCell(inTableFilterRow, 1));
}


/*======================================================================================
	Insert the specified filter into the table.
======================================================================================*/

void CFiltersTableView::RemoveFilterAt(TableIndexT inTableFilterRow, MSG_Filter **outFilter) {

	MSG_Filter *filter = GetFilterAtRow(inTableFilterRow);
	FailFiltersError(MSG_RemoveFilterAt(mFilterList, TABLE_TO_FILTER_INDEX(inTableFilterRow)));
	SaveChangedFilterList(inTableFilterRow);
	
	RemoveRows(1, inTableFilterRow, true);
	*outFilter = filter;
}


/*======================================================================================
	Move the row given by inCurrRow to the row given by inNewRow. If the row is selected,
	it remains selected.
======================================================================================*/

void CFiltersTableView::RemoveRows(Uint32 inHowMany, TableIndexT inFromRow, Boolean inRefresh) {

	STableCell selectedCell = GetFirstSelectedCell();
	Boolean cellWasSelected = IsValidCell(selectedCell);

	LTableView::RemoveRows(inHowMany, inFromRow, inRefresh);

	if ( cellWasSelected ) {
		selectedCell = GetFirstSelectedCell();
		if ( !IsValidCell(selectedCell) ) {
			// Selected cell was removed!
			if ( mEditFilterName->IsVisible() || (mEditFilterNameRow > 0) ) {
				mEditFilterName->Hide();
				mEditFilterNameRow = 0;
			}
			SelectionChanged();
		}
	}
}


/*======================================================================================
	Move the row given by inCurrRow to the row given by inNewRow. If the row is selected,
	it remains selected.
======================================================================================*/

void CFiltersTableView::MoveRow(TableIndexT inCurrRow, TableIndexT inNewRow, Boolean inRefresh) {

	Assert_(IsValidRow(inCurrRow) && IsValidRow(inNewRow));
	if ( inCurrRow == inNewRow ) return;
	
	STableCell currCell(inCurrRow, 1);
	Boolean isSelected = CellIsSelected(currCell);
	
	// Save any current filter modifications, since they will become invalid after the filter is moved
	
	BroadcastMessage(msg_SaveModifications);
	
	// Call BE to move the filter
	
	AssertFail_(mFilterList != nil);
	BEMoveFilter(TABLE_TO_FILTER_INDEX(inCurrRow), TABLE_TO_FILTER_INDEX(inNewRow), mFilterList);
	SaveChangedFilterList();
	
	// Update our table
	if ( isSelected ) {
		// Hide selections from displaying
		StVisRgn emptyVisRgn(GetMacPort());
		StopBroadcasting();	// Don't broadcast that the selection has changed, because it really hasn't
		SelectCell(STableCell(inNewRow, 1));
		StartBroadcasting();
	}
	
	if ( inRefresh ) {
		RefreshRowRange(inCurrRow, inNewRow);
	}
}


/*======================================================================================
	Adjusts the Image so that it fits within the Frame. Get rid of the flicker!
======================================================================================*/

void CFiltersTableView::ReconcileFrameAndImage(Boolean /*inRefresh*/) {

	SPoint32 oldScrollPos;
	GetScrollPosition(oldScrollPos);

	LTableView::ReconcileFrameAndImage(false);

	SPoint32 newScrollPos;
	GetScrollPosition(newScrollPos);
	
	if ( ((newScrollPos.v != oldScrollPos.v) || (newScrollPos.h != oldScrollPos.h)) ) {
		Refresh();
	}
}


/*======================================================================================
	Setup the helpers for the table.
======================================================================================*/

void CFiltersTableView::SetUpTableHelpers(void) {

	// Add helpers
	
	LFlexTableGeometry *geometry = new LFlexTableGeometry(this, mTableHeader);
	FailNIL_(geometry);
	SetTableGeometry(geometry);

	//CFilterTableSelector *selector = new LTableRowSelector(this);
	//	FailNIL_(selector);
	SetTableSelector(new LTableRowSelector(this, false));
	
	CFilterTableKeyAttachment *ketAttachment = new CFilterTableKeyAttachment(this);
	FailNIL_(ketAttachment);
	AddAttachment(ketAttachment);
}


/*======================================================================================
	Draw the cell.
======================================================================================*/

void CFiltersTableView::DrawCellContents(const STableCell &inCell, const Rect &inLocalRect)
{

	CStr255 displayText;
	ResIDT iconID;
	Int16 horizJustType;
	GetDisplayData(inCell, &displayText, &horizJustType, &iconID);
	
	Rect cellDrawFrame;
	
	if ( iconID != 0 )
	{
		::SetRect(&cellDrawFrame, 0, 0, cSmallIconWidth, cSmallIconHeight);
		UGraphicGizmos::CenterRectOnRect(cellDrawFrame, inLocalRect);
		::PlotIconID(&cellDrawFrame, ttNone, ttNone, iconID);
	} else
	{
		cellDrawFrame = inLocalRect;
		::InsetRect(&cellDrawFrame, cColTextHMargin, 0);	// For a nicer look between cells
		if ( displayText.Length() > 0 )
		{
			UGraphicGizmos::PlaceTextInRect((char *) &displayText[1], displayText[0],
											cellDrawFrame, horizJustType, teCenter, 
											&mTextFontInfo, true, truncMiddle);
		}
	}
}


/*======================================================================================
	Adjust a cell's cursor.
======================================================================================*/

void CFiltersTableView::AdjustCursorSelf(Point inPortPt, const EventRecord &inMacEvent) {

	EMouseTableLocation location = GetMouseTableLocation(inPortPt);
	
	switch ( location ) {
//		case eInEnabledCheck:
//			TrySetCursor(cEnabledCheckCursorID);
//			break;
		case eInTitle:
			TrySetCursor(iBeamCursor);
			break;
		case eInCellContent:
			TrySetCursor(cHandOpenCursorID);
			break;
		default:
			LTableView::AdjustCursorSelf(inPortPt, inMacEvent);
			break;
	}
}


/*======================================================================================
	Adjust selection in response to a click in the specified cell.
======================================================================================*/

Boolean CFiltersTableView::ClickSelect(const STableCell &inCell, const SMouseDownEvent &inMouseDown) {

	Rect localClickRect;

	EMouseTableLocation location = GetMouseTableLocation(inMouseDown.wherePort, &localClickRect);
	
	switch ( location ) {
		case eInEnabledCheck:
			ToggleFilterEnabled(inCell.row, &localClickRect);
			return false;
		case eInTitle: {
				SMouseDownEvent mouseEvent = inMouseDown;
				SwitchEditFilterName(inCell.row, &localClickRect, nil, &mouseEvent);
			}
			return false;
		default:
			LCommander::SwitchTarget(this);
			break;
	}
	
	return LTableView::ClickSelect(inCell, inMouseDown);
}


/*======================================================================================
	Click the cell.
======================================================================================*/

void CFiltersTableView::ClickCell(const STableCell &inCell, const SMouseDownEvent &inMouseDown) {

	if ( LPane::GetClickCount() == 2 ) {
	
		BroadcastMessage(msg_DoubleClickCell);
		
	} else if ( mRows > 1 ) {

		CTableRowDragger dragger(inCell.row);
		
		SPoint32 startMouseImagePt;
		LocalToImagePoint(inMouseDown.whereLocal, startMouseImagePt);
		
		// Restrict dragging to a thin vertical column the height of the image
		
		LImageRect dragPinRect;
		{
			Int32 left, top, right, bottom;
			GetImageCellBounds(inCell, left, top, right, bottom);
			dragPinRect.Set(startMouseImagePt.h, startMouseImagePt.v - top, startMouseImagePt.h, 
							mImageSize.height - (bottom - startMouseImagePt.v) + 1);
		}
		
		TrySetCursor(cHandClosedCursorID);
		dragger.DoTrackMouse(this, &startMouseImagePt, &dragPinRect, 2, 2);
			
		TableIndexT newRow = dragger.GetDraggedRow();
			
		if ( newRow ) {
			CMoveFilterAction *action = new CMoveFilterAction(this, inCell.row, newRow);
			FailNIL_(action);
			PostAction(action);
		}
	}
}


/*======================================================================================
	Determine if the mouse location is in the enabled check box.
======================================================================================*/

CFiltersTableView::EMouseTableLocation CFiltersTableView::GetMouseTableLocation(Point inPortPt, 
																				Rect *outLocationRect) {

	PortToLocalPoint(inPortPt);
	SPoint32 imagePoint;
	LocalToImagePoint(inPortPt, imagePoint);
	STableCell hitCell;
//	Rect localCellRect;
	
	EMouseTableLocation rtnVal = eInNone;
	
	if ( GetCellHitBy(imagePoint, hitCell) ) {
	
		const PaneIDT colType = GetCellDataType(hitCell);
		
		switch ( colType ) {
	
			case paneID_OrderCaption:
				rtnVal = eInCellContent;
				break;

			case paneID_NameCaption: {
					Rect checkRect;
					GetFilterTitleRect(hitCell.row, &checkRect);
					if ( ::PtInRect(inPortPt, &checkRect) ) {
						if ( outLocationRect != nil ) *outLocationRect = checkRect;
						rtnVal = eInTitle;
					} else {
						rtnVal = eInCellContent;
					}
				}
				break;
		
			case paneID_EnabledCaption:
				if ( outLocationRect ) {
					Rect localCellRect;
					GetLocalCellRect(hitCell, localCellRect);
					::SetRect(outLocationRect, 0, 0, cSmallIconWidth, cSmallIconHeight);
					UGraphicGizmos::CenterRectOnRect(*outLocationRect, localCellRect);
				}
				rtnVal = eInEnabledCheck;
				break;
		
			default:
				Assert_(false);	// Should never get here!
				break;
		}
	}
	
	return rtnVal;
}


/*======================================================================================
	Get the rect bounding the specified filter's title.
======================================================================================*/

void CFiltersTableView::GetFilterTitleRect(TableIndexT inRow, Rect *outTitleRect,
										   CStr255* /*outNameString*/) {

	STableCell filterCell(inRow, mTableHeader->ColumnFromID(paneID_NameCaption));
	
	Assert_(IsValidCell(filterCell));
	
	GetLocalCellRect(filterCell, *outTitleRect);
	::InsetRect(outTitleRect, cColTextHMargin, 0);

	GetSuperView()->EstablishPort();
	UTextTraits::SetPortTextTraits(mTextTraitsID);
	
	::InsetRect(outTitleRect, -CInlineEditField::cEditBoxMargin, 0);
}


/*======================================================================================
	Switch the current filter name being edited.
======================================================================================*/

void CFiltersTableView::SwitchEditFilterName(TableIndexT inRow, const Rect *inNameLocalFrame,
											 CStr255 *inNameString, SMouseDownEvent *ioMouseDown) {

	UpdateFilterNameChanged();
	
	if ( inRow != 0 ) {
		mEditFilterNameRow = -inRow;
		
		SPoint32 imagePoint;
		LocalToImagePoint(topLeft(*inNameLocalFrame), imagePoint);
		imagePoint.h -= 1; imagePoint.v += 1;
		
		LCommander::SwitchTarget(mEditFilterName);

		StopBroadcasting();	// Don't broadcast that the selection has changed until we update the edit field
		SelectCell(STableCell(inRow, 1));
		StartBroadcasting();
		
		CStr255 nameString;
		if ( inNameString == nil ) {
			inNameString = &nameString;
			GetFilterNameAtRow(inRow, inNameString);
		}

		mEditFilterName->UpdateEdit(*inNameString, &imagePoint, ioMouseDown);
		
		SelectionChanged();
	}
}


/*======================================================================================
	If the filter name has changed, update the filter name in the filter list.
======================================================================================*/

void CFiltersTableView::UpdateFilterNameChanged(void) {

	if ( mEditFilterName->IsVisible() || mEditFilterNameRow > 0 ) {
		// Filter is currently being edited
		TableIndexT row = mEditFilterNameRow < 0 ? -mEditFilterNameRow : mEditFilterNameRow;
		CStr255 name;
		mEditFilterName->GetDescriptor(name);
		SetFilterNameAtRow(row, &name);
		SaveChangedFilterList(row);
	}
}


/*======================================================================================
	Select the filter's name for editing.
======================================================================================*/

void CFiltersTableView::SelectEditFilterName(TableIndexT inRow) {

	Rect titleRect;
	CStr255 nameString;
	GetFilterTitleRect(inRow, &titleRect, &nameString);
	SwitchEditFilterName(inRow, &titleRect, &nameString, nil);
}


/*======================================================================================
	Delete all selected items.
======================================================================================*/

void CFiltersTableView::DeleteSelection(void) {

	BroadcastMessage(msg_DeleteCell);
}

				
/*======================================================================================
	Get the display data for the specified cell.
======================================================================================*/

void CFiltersTableView::GetDisplayData(const STableCell &inCell, CStr255 *outDisplayText,
									   Int16 *outHorizJustType, ResIDT *outIconID) {

	const PaneIDT colType = GetCellDataType(inCell);
	
	*outDisplayText = CStr255::sEmptyString;
	*outIconID = 0;
	
	switch ( colType ) {
	
		case paneID_OrderCaption: {
				Str15 numString;
				::NumToString(inCell.row, numString);
				*outDisplayText = numString;
				*outHorizJustType = /*teFlushRight*/teCenter;
			}
			break;
	
		case paneID_NameCaption: {
				if ( inCell.row != ABS_VAL(mEditFilterNameRow) ) {
					GetFilterNameAtRow(inCell.row, outDisplayText);
					*outHorizJustType = teFlushLeft;
				}
			}
			break;
	
		case paneID_EnabledCaption: {
				if ( GetFilterEnabledAtRow(inCell.row) ) {
					*outIconID = kEnabledCheckIconID;
				} else {
					*outIconID = kDisabledCheckIconID;
				}
			}
			break;
	
		default:
			Assert_(false);	// Should never get here!
			break;
	}
}


/*======================================================================================
	Toggle the enabled setting of the filter at the specified row.
======================================================================================*/

void CFiltersTableView::ToggleFilterEnabled(TableIndexT inRow, Rect *inLocalIconRect) {

	Boolean doEnable = !GetFilterEnabledAtRow(inRow);
	SetFilterEnabledAtRow(inRow, doEnable);
	
	if ( doEnable ) {
		if ( FocusExposed() ) {
			::PlotIconID(inLocalIconRect, ttNone, ttNone, kEnabledCheckIconID);
		}
	} else {
		LocalToPortPoint(topLeft(*inLocalIconRect));
		LocalToPortPoint(botRight(*inLocalIconRect));
		InvalPortRect(inLocalIconRect);
		UpdatePort();
	}
	SaveChangedFilterList(inRow);
}


/*======================================================================================
	Set the enabled setting of the filter log.
======================================================================================*/

void CFiltersTableView::SetLogFilters(Boolean inDoLog) {

	Assert_(mFilterList != nil);
	Boolean isEnabled = MSG_IsLoggingEnabled(mFilterList) ? true : false;
	if ( isEnabled != inDoLog ) {
		FailFiltersError(MSG_EnableLogging(mFilterList, inDoLog));
		SaveChangedFilterList();
	}
}


/*======================================================================================
	Remove the specified filter from the list and delete it.
======================================================================================*/

void CFiltersTableView::BEDeleteFilter(Int32 inFilterIndex, MSG_FilterList *ioFilterList) {

	// Remove and delete the old edit filter
	MSG_Filter *filter = nil;
	FailFiltersError(MSG_GetFilterAt(ioFilterList, inFilterIndex, &filter));
	AssertFail_(filter != nil);
	FailFiltersError(MSG_RemoveFilterAt(ioFilterList, inFilterIndex));
	FailFiltersError(MSG_DestroyFilter(filter));
}


/*======================================================================================
	Remove the specified filter from the list and delete it.
======================================================================================*/

void CFiltersTableView::BEMoveFilter(Int32 inFromIndex, Int32 inToIndex, MSG_FilterList *ioFilterList) {

	AssertFail_(inFromIndex != inToIndex);
	
	Int32 increment = (inFromIndex > inToIndex) ? -1 : 1;
	MSG_FilterMotion motion = (increment < 0) ? filterUp : filterDown;
	
	for (Int32 curRow = inFromIndex; curRow != inToIndex; curRow += increment) {
		FailFiltersError(MSG_MoveFilterAt(ioFilterList, curRow, motion));
	}
}


/*======================================================================================
	Raise a relevant exception if MSG_SearchError is not SearchError_Success.
======================================================================================*/

void CFiltersTableView::FailFiltersError(MSG_FilterError inError) {

	if ( inError == FilterError_Success ) return;
	
	switch ( inError ) {
	
		case FilterError_OutOfMemory:
			Throw_(memFullErr);
			break;

		case FilterError_NotImplemented: {
#ifdef Debug_Signal
				CStr255 errorString;
				USearchHelper::AssignUString(uStr_FilterFunctionNotImplemented, errorString);
				ErrorManager::PlainAlert((StringPtr) errorString);
#endif // Debug_Signal
				Throw_(unimpErr);
			}
			break;

		case FilterError_InvalidVersion:
		case FilterError_InvalidIndex:
		case FilterError_InvalidMotion:
		case FilterError_InvalidFilterType:
		case FilterError_NullPointer:
		case FilterError_NotRule:
		case FilterError_NotScript:
		case FilterError_SearchError: {
#ifdef Debug_Signal
				CStr255 errorString;
				USearchHelper::AssignUString(uStr_FilterFunctionParamError, errorString);
				ErrorManager::PlainAlert((StringPtr) errorString);
#endif // Debug_Signal
				Throw_(paramErr);
			}
			break;
			
		case FilterError_FileError:
			Throw_(ioErr);
			break;

		default:
			AssertFail_(false);
			Throw_(32000);	// Who knows?
			break;
			
	}
}
