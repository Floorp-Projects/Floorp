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

#include "CPrefsMediator.h"

#include "prefapi.h"

#include "macutil.h"
#include "ufilemgr.h"

#include "MPreference.h"
#include "CSizePopup.h"
#ifdef MOZ_MAIL_NEWS
#include "CMessageFolder.h"
#endif
#include "StSetBroadcasting.h"
#include "InternetConfig.h"
#include "CMouseDragger.h"

Boolean	CPrefsMediator::sUseIC = false;
LView *CPrefsMediator::sPanel = nil;
LWindow	*CPrefsMediator::sWindow = nil;
CPrefsDialog::Selection_T CPrefsMediator::sInitialSelection
	= CPrefsDialog::eIgnore;

#pragma mark ---CControlExtension---
//======================================
class CControlExtension: LCaption
//======================================
{
	public:

		enum
		{
			class_ID = 'CtlX'
		};

						CControlExtension(LStream *inStream);
		virtual			~CControlExtension() {}

		virtual void	ClickSelf(const SMouseDownEvent& inMouseDown);

		virtual	void	FinishCreateSelf();

	protected:
		PaneIDT			mControlID;
		LControl		*mControl;
}; // class CControlExtension

//----------------------------------------------------------------------------------------
CControlExtension::CControlExtension(LStream *inStream):
	LCaption(inStream),
	mControl(nil)
//----------------------------------------------------------------------------------------
{
	*inStream >> mControlID;
} // CControlExtension::CControlExtension

//----------------------------------------------------------------------------------------
void CControlExtension::FinishCreateSelf()
//----------------------------------------------------------------------------------------
{
	LPane	*pane = this;
	LView	*superView, *topView = nil;
	#pragma warn_possunwant off
	while (superView = pane->GetSuperView())
	#pragma warn_possunwant reset
	{
		topView = superView;
		pane = superView;
	}
	XP_ASSERT(topView);
	if (topView)
	{
		mControl = (LControl *)topView->FindPaneByID(mControlID);
	}
	XP_ASSERT(mControl);
} // CControlExtension::FinishCreateSelf

//----------------------------------------------------------------------------------------
void CControlExtension::ClickSelf(const SMouseDownEvent& /*inMouseDown*/)
//----------------------------------------------------------------------------------------
{
	if (mControl)
		mControl->SimulateHotSpotClick(1);	// any value > 0 && < 128 should work
} // CControlExtension::ClickSelf

#if 0

#pragma mark ---class CControlExtensionAttachment---
//======================================
class CControlExtensionAttachment: LAttachment
//======================================
{
	public:
		CControlExtensionAttachment(LControl *targetControl);
		virtual	~CControlExtensionAttachment() {}

	protected:
		LControl		*mControl;

		virtual void	ExecuteSelf(MessageT inMessage, void *ioParam);
}; // class CControlExtensionAttachment

//----------------------------------------------------------------------------------------
CControlExtensionAttachment::CControlExtensionAttachment(LControl* targetControl)
//----------------------------------------------------------------------------------------
:	LAttachment(msg_Click, true)
,	mControl(targetControl)
{
}

//----------------------------------------------------------------------------------------
void CControlExtensionAttachment::ExecuteSelf(MessageT inMessage, void* ioParam)
//----------------------------------------------------------------------------------------
{
	if (mControl)
		mControl->SimulateHotSpotClick(1);	// any value > 0 && < 128 should work
}
#endif	// 0

#ifdef MOZ_MAIL_NEWS
#pragma mark ---CPopupFolderMenu---

//======================================
class CPopupFolderMenu : public LGAPopup, public CGAPopupFolderMixin
//======================================
{
	public:
		enum
		{
			class_ID = 'PupF'
		};

						CPopupFolderMenu(LStream	*inStream);
	virtual				~CPopupFolderMenu();
	virtual void		FinishCreateSelf();
	virtual	Boolean		FolderFilter(const CMessageFolder &folder);

//	private:
// It turns out that this causes a crash in some thread code. Apparently someone
// is counting on the MSG_Master not going away
//		CMailNewsContext	mMailNewsContext;
			// OK, what the heck is this for? Well the CPopupFolderMenus that we
			// create will call CMailFolderMixin::UpdateMailFolderMixinsNow() in
			// FinishCreateSelf(). This call will call CMailNewsContext::GetMailMaster().
			// CMailNewsContext::GetMailMaster() will  create a MSG_Master if the static
			// MSG_Master does not already exist. This means that the code would work
			// without mMailNewsContext, but the static MSG_Master would continue to exist
			// after we are finished. This is not good if the user is not using mail. So
			// we create a CMailNewsContext because its contrustor will create the
			// the static MSG_Master but it is ref counted and destroyed in
			// CMailNewsContext::~CMailNewsContext.
}; // class CPopupFolderMenu

//----------------------------------------------------------------------------------------
CPopupFolderMenu::CPopupFolderMenu(LStream	*inStream)
//----------------------------------------------------------------------------------------
:	LGAPopup(inStream)
{
	mDesiredFolderFlags = eWantPOP;
}

//----------------------------------------------------------------------------------------
CPopupFolderMenu::~CPopupFolderMenu()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void CPopupFolderMenu::FinishCreateSelf()
//----------------------------------------------------------------------------------------
{
	LGAPopup::FinishCreateSelf();
	CMailFolderMixin::UpdateMailFolderMixinsNow(this);
}

//----------------------------------------------------------------------------------------
Boolean CPopupFolderMenu::FolderFilter(const CMessageFolder& folder)
//----------------------------------------------------------------------------------------
{
	Boolean	result = CGAPopupFolderMixin::FolderFilter(folder);
	// We want to exclude the Outbox.
	if (result && (folder.GetFolderFlags() & MSG_FOLDER_FLAG_QUEUE))
	{
		result = false;
	}
	return result;
}
#endif // MOZ_MAIL_NEWS

#pragma mark ---CPrefsMediator---

//----------------------------------------------------------------------------------------
CPrefsMediator::CPrefsMediator(PaneIDT inMainPaneID)
//----------------------------------------------------------------------------------------
:	mMainPane(nil)
,	mNeedsPrefs(true)
,	mMainPaneID(inMainPaneID)
,	mLatentSub(nil)
{
	MPreferenceBase::SetWriteOnDestroy(false);
}

//----------------------------------------------------------------------------------------
CPrefsMediator::~CPrefsMediator()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::LoadPrefs()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::WritePrefs()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::StepAside()
//----------------------------------------------------------------------------------------
{
	if (mMainPane)
	{
		mLatentSub = LCommander::GetTarget();
		SwitchTarget(nil); // take current chain off duty, so that SetLatentSub() can work.
		mMainPane->Hide();
	}
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::LoadPanes()
//----------------------------------------------------------------------------------------
{
	if (!mMainPane)
		LoadMainPane();
	mMainPane->Hide();	// this is needed for IC support
						// when panes are loaded, but not immediately displayed
	if (mNeedsPrefs)
	{
		LoadPrefs();
		mNeedsPrefs = false;
	}
	UpdateFromIC();
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::StepForward(LCommander *defaultSub)
//----------------------------------------------------------------------------------------
{
	TrySetCursor(watchCursor);
	Boolean firstTime = (mMainPane == nil);
	if (!firstTime && mLatentSub != sWindow)
		sWindow->SetLatentSub(mLatentSub);	// restore latent sub from previously hidden pane.
											// In the first-time case,  it's done in FinishCreate().
	LoadPanes();
	if (mMainPane)
	{
		mMainPane->Show();
		mLatentSub = sWindow->GetLatentSub();
		if (!mLatentSub)
			mLatentSub = defaultSub;
		SwitchTarget(mLatentSub);
	}
//	if (firstTime)
//		SelectFirstEnabledEditField();		// On subsequent times, use the user's selection?
			// Actually, don't do this.  The edittext pref resources all stand up and say which
			// one wants to be the target.
	SetCursor(&qd.arrow);
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::LoadMainPane()
//----------------------------------------------------------------------------------------
{
	sWindow->UpdatePort();
		// The previous contents have been "hidden" but they are still visible. If we
		// did not call UpdatePort() then the new content would appear over the old,
		// but that is OK because everything gets straightened out at the next update.
		// The only problem is if a panel is slow to draw itself-- the
		// Mail & News / Messages pane is a good example--then we can actually see both
		// the old and the new contents at the same time. Bad thing.
	PaneIDT	paneID = GetMainPaneID();
	mMainPane = UReanimator::CreateView(paneID, sPanel, sWindow);
	UReanimator::LinkListenerToControls(this, mMainPane, paneID);
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::Canceled()
//----------------------------------------------------------------------------------------
{
	mNeedsPrefs = true;
	MPreferenceBase::SetWriteOnDestroy(false);
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::ListenToMessage(MessageT inMessage, void */*ioParam*/)
//----------------------------------------------------------------------------------------
{
	switch (inMessage)
	{
		case eCommitPrefs:
			WritePrefs();
			MPreferenceBase::SetWriteOnDestroy(true);
			break;
		case eCancelPrefs:
			Canceled();
			break;
		default:
			break;
	}
}

//----------------------------------------------------------------------------------------
MPreferenceBase* CPrefsMediator::GetPreferenceObject(PaneIDT inPaneID) const
//----------------------------------------------------------------------------------------
{
	LPane* p = FindPaneByID(inPaneID);
	MPreferenceBase* pb = dynamic_cast<MPreferenceBase*>(p);
	SignalIf_(!pb);
	return pb;
}

//----------------------------------------------------------------------------------------
Boolean CPrefsMediator::PaneHasLockedPref(PaneIDT inPaneID) const
//----------------------------------------------------------------------------------------
{
	MPreferenceBase* pb = GetPreferenceObject(inPaneID);
	if (pb)
		return pb->IsLocked();
	return true;
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::ReadDefaultPref(PaneIDT inPaneID)
//----------------------------------------------------------------------------------------
{
	MPreferenceBase* pb = GetPreferenceObject(inPaneID);
	if (pb)
		pb->ReadDefaultSelf();
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::ChangePrefName(PaneIDT inPaneID, const char* inNewName)
//----------------------------------------------------------------------------------------
{
	MPreferenceBase::ChangePrefName(mMainPane, inPaneID, inNewName);
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::SetValidationFunction(
	PaneIDT inPaneID,
	CValidEditField::ValidationFunc inFunc) const
//----------------------------------------------------------------------------------------
{
	CValidEditField* p =
		(CValidEditField*)FindPaneByID(inPaneID);
	p = dynamic_cast<CValidEditField*>(p);
	Assert_(p); // only makes sense for CValidEditField.
	if (p)
		p->SetValidationFunction(inFunc);
}

// static
//----------------------------------------------------------------------------------------
Boolean CPrefsMediator::CanSwitch(LCommander *inNewTarget)
//----------------------------------------------------------------------------------------
{
	CValidEditField	*target = dynamic_cast<CValidEditField	*>(LCommander::GetTarget());
	if (target)
		return target->AllowTargetSwitch(inNewTarget);
	return true;
}


//----------------------------------------------------------------------------------------
// static
void CPrefsMediator::SelectFirstEnabledEditField()
//----------------------------------------------------------------------------------------
{
	LEditField *currentTarget = dynamic_cast<LEditField *>(LCommander::GetTarget());
	if (currentTarget)
	{
		if (!currentTarget->IsEnabled())
		{
			LTabGroup *tabGroup =
					dynamic_cast<LTabGroup *>(currentTarget->GetSuperCommander());
			if (tabGroup)
			{
				tabGroup->RotateTarget(false /*not backward*/);
				LEditField	*oldTarget = currentTarget;
				currentTarget = dynamic_cast<LEditField *>(LCommander::GetTarget());
				if (currentTarget)
				{
					if (currentTarget == oldTarget)
					{
						// This means that the tab group couldn't find any edit field
						// that was not disabled. So we need to take this guy off duty.
						currentTarget->SwitchTarget(nil);
					}
				}
				// else what the heck does this mean?
			}
			if (currentTarget && currentTarget->IsEnabled())
			{
				currentTarget->SelectAll();
			}
		}
	}
}

enum
{
	eNNTPStandardPort	= 119,
	eNNTPSecurePort	= 563
};

//----------------------------------------------------------------------------------------
void CPrefsMediator::SetEditFieldsWithICPref(ConstStr255Param prefName,
										PaneIDT editFieldID,
										Boolean	stripPortNumber,
										PaneIDT	portFieldID,
										Boolean	portLocked,
										long	/*portNumber*/)
	// If stripPortNumber is false then portFieldID and portLocked are ignored.
	// If stripPortNumber is true then the first colon and any chars following it are stripped.
	// If portFieldID is non-zero and portLocked is false, then the port value is set.
//----------------------------------------------------------------------------------------
{
	if (!PaneHasLockedPref(editFieldID))
	{
		LEditField	*editField = (LEditField *)FindPaneByID(editFieldID);
		assert(editField);
		if (UseIC())
		{
			Str255	s;
			long	portNumber = eNNTPStandardPort;
			long	*portNumberPtr = stripPortNumber ? &portNumber: nil;

			CInternetConfigInterface::GetInternetConfigString(prefName, s, portNumberPtr);
			editField->SetDescriptor(s);
			editField->Disable();
			if (stripPortNumber && portFieldID && !portLocked)
			{
				LEditField	*portField = (LEditField *)FindPaneByID(portFieldID);
				assert(portField);
				portField->SetValue(portNumber);
				portField->Disable();
			}
		}
		else
		{
			editField->Enable();
			if (stripPortNumber && portFieldID && !portLocked)
			{
				LEditField	*portField = (LEditField *)FindPaneByID(portFieldID);
				assert(portField);
				portField->Enable();
			}
		}
	}
}

//----------------------------------------------------------------------------------------
int CPrefsMediator::SetFontSizePopupWithIntPref(const char *prefName,
											PaneIDT popupID,
											Boolean &locked,
											int defaultValue)
//----------------------------------------------------------------------------------------
{
	int32	result = defaultValue;
	int prefError = PREF_NOERROR;
	CSizePopup	*thePopup = (CSizePopup *)FindPaneByID(popupID);
	XP_ASSERT(thePopup);
	prefError = PREF_GetIntPref(prefName, &result);
	locked = PREF_PrefIsLocked(prefName);

	if (prefError == PREF_NOERROR)
	{
		thePopup->SetFontSize(result);
	}
	if (locked)
	{
		thePopup->Disable();
	}
	return result;
}

#ifdef MOZ_MAIL_NEWS
//----------------------------------------------------------------------------------------
void CPrefsMediator::SetFolderPopupWithPref(	const char *prefName,
											PaneIDT popupID,
											Boolean& locked,
											const char* /*defaultValue*/)
//----------------------------------------------------------------------------------------
{
	CPopupFolderMenu	*thePopup =
		(CPopupFolderMenu *)FindPaneByID(popupID);
	XP_ASSERT(thePopup);
	// XP prefs: Read alias as binary type
	AliasHandle aliasH = NULL;
	int size;
	void* alias;
	if (PREF_CopyBinaryPref(prefName, &alias, &size ) == 0)
	{
		PtrToHand(alias, &(Handle)aliasH, size);
		XP_FREE(alias);
	}

	FSSpec	target;
	Boolean	wasChanged;
	OSErr err = ResolveAlias(nil, aliasH, &target, &wasChanged);
	DisposeHandle((Handle)aliasH);

	if (!err)
	{
		locked = PREF_PrefIsLocked(prefName);

		// alias to path name

		char* pathName = CFileMgr::EncodedPathNameFromFSSpec(target, true);
		if (pathName && *pathName)
		{
			thePopup->MSetSelectedFolderName(pathName);
		}
		XP_FREEIF(pathName);
		if (locked)
		{
			thePopup->Disable();
		}
	}
	return;
}
#endif // MOZ_MAIL_NEWS

//----------------------------------------------------------------------------------------
Boolean CPrefsMediator::SetEditFieldWithLocalFileURL(PaneIDT editFieldID, Boolean /*locked*/)
//----------------------------------------------------------------------------------------
{
	StandardFileReply	spec;

	

	Boolean	result = CFilePicker::DoCustomGetFile(
											spec,
											CFilePicker::TextFiles,
											false);
	if (result)
	{
		char *url = CFileMgr::GetURLFromFileSpec(spec.sfFile);

		if (url)
		{
			LEditField	*theField = (LEditField *)FindPaneByID(editFieldID);
			XP_ASSERT(theField);
			LStr255	pString(url);
			theField->SetDescriptor(pString);
			theField->SelectAll();
			XP_FREE(url);
		}
	}
	return result;
}

//----------------------------------------------------------------------------------------
void CPrefsMediator::SetIntPrefWithFontSizePopup(	const char *prefName,
												PaneIDT popupID,
												Boolean locked)
//----------------------------------------------------------------------------------------
{
	if (!locked)
	{
		CSizePopup *thePopup = (CSizePopup *)FindPaneByID(popupID);
		XP_ASSERT(thePopup);
		Int32	popupValue		= thePopup->GetFontSize();
		PREF_SetIntPref(prefName, popupValue);
	}
}

#ifdef MOZ_MAIL_NEWS
//----------------------------------------------------------------------------------------
void CPrefsMediator::SetPrefWithFolderPopup(	const char *prefName,
											PaneIDT popupID,
											Boolean locked)
//----------------------------------------------------------------------------------------
{
	if (!locked)
	{
		CPopupFolderMenu	*thePopup =
			(CPopupFolderMenu *)FindPaneByID(popupID);
		XP_ASSERT(thePopup);
		const char* folderName = thePopup->MGetSelectedFolderName();

		// drop out if no folder is selected (b2 patch; the popups should really have
		// non-empty defaults but this may also happen if folder doesn't exist anymore?)
		if (!folderName || !*folderName)
			return;

		// make alias
		FSSpec	fileSpec;
		OSErr	iErr = CFileMgr::FSSpecFromLocalUnixPath(folderName, &fileSpec);

		if (!iErr)
		{
			AliasHandle	aliasH;
			iErr = NewAlias(nil, &fileSpec, &aliasH);
			if (!iErr)
			{
				Size	lByteCnt = GetHandleSize((Handle)aliasH);
				HLock((Handle)aliasH);
				PREF_SetBinaryPref(prefName, *aliasH, lByteCnt);
				DisposeHandle((Handle)aliasH);
			}
		}
	}
}
#endif // MOZ_MAIL_NEWS

//----------------------------------------------------------------------------------------
void CPrefsMediator::ActivateHelp( )
// Called when the user clicks on the "Help" button in the Prefs window. Apart from working
// out the help name from the PPob resource name, this is just a pass-through
// to the appropriate FE call with the current help topic for this pane.
//----------------------------------------------------------------------------------------
{
	// The help string is the resource name of the 'PPob' resource for the help pane.  
	::SetResLoad(false);
	Handle h = ::GetResource('PPob', mMainPaneID);
	if (!h)
		return;
	::SetResLoad(true);
	short idIgnored; ResType resTypeIgnored;
	CStr255 resName;
	::GetResInfo(h, &idIgnored, &resTypeIgnored, resName);
	if (!*h)
		::ReleaseResource(h); // if master pointer is not nil, it was not we who loaded it!
	if (resName.Length() == 0)
		return;
	// The pref strings are long and redundant.  This means that the significant part
	// is not visible in Constructor.  So I replaced the two common substrings "avigat" and
	// "r:PREFERENCES_" with "É" and I'm putting them back in here.  Example:
	// resource name "nÉAPPEARANCE" becomes help name "navigatr:PREFERENCES_APPEARANCE"
	// resource name "cÉEDITOR_GENERAL" becomes help name "composer:PREFERENCES_EDITOR_GENERAL"
	char fullhelpname[256];
	Assert_(resName[2] == (unsigned char)'É');
	if (resName[2] != (unsigned char)'É')
		return; // No help available for this topic
	switch (resName[1])
	{
		case 'n':
			XP_STRCPY(fullhelpname, "navigat");
			break;
		case 'c':
			XP_STRCPY(fullhelpname, "compose");
			break;
		case 'm':
			XP_STRCPY(fullhelpname, "messeng");
			break;
		default:
			Assert_(false);
			return;
	}
	XP_STRCAT(fullhelpname, "r:PREFERENCES_");
	XP_STRCAT(fullhelpname, 2 + (const char*)resName);
	ShowHelp(fullhelpname);
} // ActivateHelp

//----------------------------------------------------------------------------------------
void CDragOrderTextList::MoveRow(TableIndexT inCurrRow, TableIndexT inNewRow)
//----------------------------------------------------------------------------------------
{
	Assert_(IsValidRow(inCurrRow) && IsValidRow(inNewRow));
	if (inCurrRow == inNewRow)
		return;

	{ // -- broadcasting scope
		STableCell currCell(inCurrRow, 1);
		StSetBroadcasting(this, false);
		// Don't broadcast that the selection has changed, because it really hasn't
		Uint32	ioDataSize;
		GetCellData(currCell, nil, ioDataSize);
		if (ioDataSize)
		{
			char *temp = (char *)XP_ALLOC(ioDataSize);
			if (temp)
			{
				GetCellData(currCell, temp, ioDataSize);
				RemoveRows(1, inCurrRow, false);
				InsertRows(1, inNewRow - 1, temp, ioDataSize, true);
				XP_FREE(temp);
			}
		}
	} // -- broadcasting scope
	BroadcastMessage(msg_ChangedOrder);
	SelectCell(STableCell(inNewRow, 1));
	Refresh();
}

//----------------------------------------------------------------------------------------
void CDragOrderTextList::ClickCell(	const STableCell &inCell,
							const SMouseDownEvent &inMouseDown)
//----------------------------------------------------------------------------------------
{
	if ( LPane::GetClickCount() == 2 )
	{
		if (mDoubleClickMsg != msg_Nothing)
		{
			BroadcastMessage(mDoubleClickMsg, (void*) this);
		}
	}
	else if (mRows > 1 && !mOrderLocked)
	{
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
		TrySetCursor(eHandClosedCursorID);
		dragger.DoTrackMouse(this, &startMouseImagePt, &dragPinRect, 2, 2);
			
		TableIndexT newRow = dragger.GetDraggedRow();
			
		if (newRow)
		{
			MoveRow(inCell.row, newRow);
//			CMoveFilterAction *action = new CMoveFilterAction(this, inCell.row, newRow);
//			FailNIL_(action);
//			PostAction(action);
		}
	}
} // CDragOrderTextList::ClickCell

//----------------------------------------------------------------------------------------
void CPrefsMediator::RegisterViewClasses()
//----------------------------------------------------------------------------------------
{
#ifdef MOZ_MAIL_NEWS
	RegisterClass_(CPopupFolderMenu);
#endif
	RegisterClass_( CControlExtension);
	RegisterClass_(CDragOrderTextList);
}

