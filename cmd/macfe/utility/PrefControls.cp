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

#include "PrefControls.h"

#include "MPreference.h"
#include "CValidEditField.h"

#include "prefwutil.h"
#include "prefapi.h"
#include "ufilemgr.h"
#include "StSetBroadcasting.h"

#include "xp_mcom.h"

#include <LCheckbox.h>
#include <LRadioButton.h>
//#include <LPopupButton.h>
#include <LGAPopup.h>


// Note.	We can do java-style 'inline' virtual functions here, because these classes
//			all have file scope.  If you ever move these declarations to a header, you
//			should non-inline 'em.

#pragma mark ---CPrefCheckbox---
//======================================
class CPrefCheckbox
//======================================
:	public LCheckBox
,	public MPreference<LControl,XP_Bool>
{
public:
	CPrefCheckbox(LStream* inStream)
		:	LCheckBox(inStream)
		,	MPreference<LControl,XP_Bool>(this, inStream)
		{
		}
	virtual ~CPrefCheckbox()
		{
			WriteSelf();
				// checkbox always writes its pane value ^ its ordinal.
			 	// Thus: if mOrdinal == 1, the checkbox reverses the sense of the pref.
		}
	enum { class_ID = 'PCHK' };
	virtual void FinishCreateSelf()
		{
			LCheckBox::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
		}
}; // class CPrefCheckbox

#pragma mark ---CBoolPrefRadio---
//======================================
class CBoolPrefRadio
// Actually, because of the writing-out policy, only the zero-ordinal radio button
// needs to be a CBoolPrefRadio.  The other one can be just a LRadioButton.
//======================================
:	public LRadioButton
,	public MPreference<LControl,XP_Bool>
{
public:
	CBoolPrefRadio(LStream* inStream)
		:	LRadioButton(inStream)
		,	MPreference<LControl,XP_Bool>(this, inStream)
		{
		}
	virtual ~CBoolPrefRadio()
		{
			// Only the radio button with a zero ordinal writes its value out.
			// It writes out its pane value.
			if (mOrdinal == 0)
				WriteSelf();
		}
	enum { class_ID = 'BRAD' };
	virtual void FinishCreateSelf()
		{
			LRadioButton::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
		}
}; // class CBoolPrefRadio

#pragma mark ---CIntPrefRadio---
//======================================
class CIntPrefRadio
//======================================
:	public LRadioButton
,	public MPreference<LControl,int32>
{
public:
	CIntPrefRadio(LStream* inStream)
		:	LRadioButton(inStream)
		,	MPreference<LControl,int32>(this, inStream)
		{
		}
	virtual ~CIntPrefRadio()
		{
			// Only the radio button with an ON value writes its value out.
			// An int preference writes out its ordinal.
			if (mPaneSelf->GetValue() != 0)
				WriteSelf();
		}
	enum { class_ID = 'IRAD' };
	virtual void FinishCreateSelf()
		{
			LRadioButton::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
		}
}; // class CIntPrefRadio

#pragma mark ---CPrefColorButton---
//======================================
class CPrefColorButton
//======================================
:	public CColorButton
,	public MPreference<CColorButton,RGBColor>
{
public:
	enum { class_ID = 'Pcol' };
	CPrefColorButton(LStream* inStream)
		:	CColorButton(inStream)
		,	MPreference<CColorButton,RGBColor>((CColorButton*)this, inStream)
		{
		}
	virtual ~CPrefColorButton()
		{
			WriteSelf();
		}
	virtual void FinishCreateSelf()
		{
			CColorButton::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
		}
}; // class CPrefColorButton

#pragma mark ---CIntPrefPopup---
//======================================
class CIntPrefPopup
// NOTE: There are two modes of operation: command mode and item mode.
// Command mode happens if mNumCommands > 0, otherwise item mode obtains.
// In item mode, unlike the base class LGAPopup, the values are zero based.  I.e., the
// first menu item represents a pref value of zero.
// In command mode, the command numbers of the items are used for the pref values,
// allowing the menu items to represent non-consecutive pref values, or reordered
// pref values.
//======================================
:	public LGAPopup
,	public MPreference<LControl,int32>
{
public:
	CIntPrefPopup(LStream* inStream)
		:	LGAPopup(inStream)
		,	MPreference<LControl,int32>(this, inStream)
		{
		}
	virtual ~CIntPrefPopup()
		{
			WriteSelf();
		}
	enum { class_ID = 'Ppop' };
	virtual void FinishCreateSelf()
		{
			LGAPopup::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
		}
	virtual int32 GetPaneValue() const
		{
			Int32 itemNumber = LGAPopup::GetValue();
			// Use the command nums, if provided.
			if (mNumCommands > 0)
				return (*mCommandNums)[itemNumber - 1];
			// Otherwise, use the menu item number - 1 (it's zero based).
			return itemNumber - 1;
		}
	virtual void SetPaneValue(int32 inData)
		{
			if (mNumCommands == 0)
			{
				// Item mode.
				LGAPopup::SetValue(inData + 1); // first item's value is zero.
			}
			else
			{
				// Command mode.
				CommandT* cur = *mCommandNums;
				for (int item = 1; item <= mNumCommands; item++, cur++)
				{
					if (*cur == inData)
					{
						SetValue(item);
						break;
					}
				}
			}
		}

}; // class CIntPrefPopup

#pragma mark ---CTextPrefPopup---
//======================================
class CTextPrefPopup
//======================================
:	public LGAPopup
,	public MPreference<LGAPopup,char*>
{
public:
	CTextPrefPopup(LStream* inStream)
		:	LGAPopup(inStream)
		,	MPreference<LGAPopup,char*>(this, inStream)
		{
		}
	virtual ~CTextPrefPopup()
		{
			WriteSelf();
		}
	enum { class_ID = 'Tpop' };
	virtual void FinishCreateSelf()
		{
			LGAPopup::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
		}
}; // class CTextPrefPopup

#pragma mark ---CPrefEditText---
//======================================
class CPrefTextEdit
//======================================
:	public CValidEditField
,	public MPreference<LTextEdit,char*>
{
public:
	CPrefTextEdit(LStream* inStream)
		:	CValidEditField(inStream)
		,	MPreference<LTextEdit,char*>((LTextEdit*)this, inStream)
		{
		}
	virtual ~CPrefTextEdit()
		{
			// A textedit field always writes its value out.
			WriteSelf();
		}
	enum { class_ID = 'PEDT' };
	virtual void FinishCreateSelf()
		{
			CValidEditField::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
			// The edit field with mOrdinal == 1 should become target:
			if (mOrdinal == 1)
				((CValidEditField*)mPaneSelf)->GetSuperCommander()->SetLatentSub(this);
		}
}; // class CPrefTextEdit

#pragma mark ---CIntPrefTextEdit---
//======================================
class CIntPrefTextEdit
// // Note: use CPrefTextEdit template in constructor, just change the class ID to 'Iedt'.
//======================================
:	public CValidEditField
,	public MPreference<LTextEdit,int32>
{
public:
	enum { class_ID = 'IEDT' };
	CIntPrefTextEdit(LStream* inStream)
		:	CValidEditField(inStream)
		,	MPreference<LTextEdit,int32>((LTextEdit*)this, inStream)
		{
		}
	virtual ~CIntPrefTextEdit()
		{
			// A textedit field always writes its value out.
			WriteSelf();
		}
	virtual void FinishCreateSelf()
		{
			CValidEditField::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
			// The edit field with mOrdinal == 1 should become target:
			if (mOrdinal == 1)
				((CValidEditField*)mPaneSelf)->GetSuperCommander()->SetLatentSub(this);
		}
}; // class CIntPrefTextEdit

#pragma mark ---CPrefFilePicker---
//======================================
class CPrefFilePicker
// mOrdinal determines the PickEnum.
//======================================
:	public CFilePicker
,	public MPreferenceBase // binary data
{
public:
	enum { class_ID = 'Pfpk' };
	typedef int	(*PrefReadFunc)(const char*, void**, int*);
	CPrefFilePicker(LStream* inStream)
		:	CFilePicker(inStream)
		,	MPreferenceBase((CFilePicker*)this, inStream)
		{
			CFilePicker::SetPickType((PickEnum)mOrdinal);
		}
	virtual ~CPrefFilePicker()
		{
			if (ShouldWrite())
			{
				if (CFilePicker::WasSet())
				{
					FSSpec fileSpec = CFilePicker::GetFSSpec();
					AliasHandle a = NULL;
					OSErr iErr = NewAlias(nil, &fileSpec, &a);
					if (!iErr)
					{
						Size iByteCnt = GetHandleSize((Handle)a);
						HLock((Handle)a);
						PREF_SetBinaryPref(mName, *a, iByteCnt);
						DisposeHandle((Handle)a);
					}
				}
			}
		}
	virtual Boolean Changed() const
		{
			return CFilePicker::WasSet();
		}
	virtual void FinishCreateSelf()
		{
			CFilePicker::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
		}
	virtual void InitializeUsing(PrefReadFunc inFunc)
		{
			// Read old alias pref from resource
			// XP prefs: Read alias as binary type
			AliasHandle alias = nil;
			int size;
			void* a;
			if (inFunc(mName, &a, &size ) == 0)
			{
				PtrToHand(a, &(Handle)alias, size);
				XP_FREE(a);
			}
			if (alias && *alias)
			{
				FSSpec fileSpec;
				fileSpec.vRefNum = fileSpec.parID = -1;
				fileSpec.name[0] = '\0';

				Boolean	ignore;
				OSErr err = ::ResolveAlias(NULL, alias, &fileSpec, &ignore);
				if (err != noErr)
					fileSpec.vRefNum = fileSpec.parID = -1;
				if (CFileMgr::FileExists(fileSpec))
				{
					StSetBroadcasting dontBroadcast((CFilePicker*)this, false);
					CFilePicker::SetFSSpec(fileSpec);
				}
			}
			if (alias)
				DisposeHandle((Handle)alias);
		}
	virtual void ReadSelf()
		{
			InitializeUsing(PREF_CopyBinaryPref);
		}
	virtual void ReadDefaultSelf()
		{
			if (!IsLocked())
				InitializeUsing(PREF_CopyDefaultBinaryPref);
		}
}; // class CPrefFilePicker

#ifdef MOZ_MAIL_NEWS
#include "macutil.h"
#include "UNewFolderDialog.h"
#include "CMessageFolder.h"

//======================================
class CSpecialFolderCheckbox
// Like a pref checkbox, but the descriptor is derived from a second, text preference.
// The second pref name is obtained from the main (boolean) one by replacing the substring
// "use" by the substring "default", eg
//		mail.use_fcc -> mail.default_fcc
// This conversion allows us to share the constructor template.  This guy can just have
// a different class ID.
//======================================
:	public CPrefCheckbox
{
private:
	typedef CPrefCheckbox Inherited;
public:
	enum { class_ID = 'FLck' };
					CSpecialFolderCheckbox(LStream* inStream);
	virtual			~CSpecialFolderCheckbox();
	virtual void	FinishCreateSelf();
	void			SetTitleUsing(const CStr255& folderName, const CStr255& serverName);

protected:
	char*	mCaptionPrefName;
	char*	mFormatString;
}; // class CSpecialFolderCheckbox

#pragma mark ---CSpecialFolderCheckbox---

//-----------------------------------
CSpecialFolderCheckbox::CSpecialFolderCheckbox(LStream* inStream)
//-----------------------------------
:	CPrefCheckbox(inStream)
,	mCaptionPrefName(nil)
,	mFormatString(nil)
{
	LStr255 originalName = mName;
	UInt8 position = originalName.Find(".use_", 1);
	originalName.Replace(position, strlen(".use_"), ".default_", strlen(".default_"));
	mCaptionPrefName = XP_STRDUP((const char*)CStr255(ConstStringPtr(originalName)));
	// The initial descriptor is the format string.
	CStr255 text;
	GetDescriptor(text);
	mFormatString = XP_STRDUP((char*)text);
}

//-----------------------------------
CSpecialFolderCheckbox::~CSpecialFolderCheckbox()
//-----------------------------------
{
	XP_FREEIF(mCaptionPrefName);
	XP_FREEIF(mFormatString);
}

//-----------------------------------
void CSpecialFolderCheckbox::FinishCreateSelf()
//-----------------------------------
{
	Inherited::FinishCreateSelf();
	
	CStr255 folderName;
	CMessageFolder folder, server;
	UFolderDialogs::GetFolderAndServerNames(
					mCaptionPrefName,
					folder,
					folderName,
					server);
	
	SetTitleUsing(folderName, server.GetPrettyName());
}

//-----------------------------------
void CSpecialFolderCheckbox::SetTitleUsing(
	const CStr255& folderName,
	const CStr255& serverName)
//-----------------------------------
{
	CStr255 title(mFormatString);
	StringParamText(title, folderName, serverName);
	SetDescriptor(title);
}

//-----------------------------------
void UPrefControls::NoteSpecialFolderChanged(
	LControl* inControl,
	int inKind,
	const CMessageFolder& inFolder)
// The control is a checkbox displaying (e.g.) "Sent Mail on FooServer". This routine updates
// the title after the choice of folder or server has changed.
//-----------------------------------
{
	CSpecialFolderCheckbox* cb = dynamic_cast<CSpecialFolderCheckbox*>(inControl);
	if (cb)
	{
		CStr255 folderName;
		CMessageFolder server;
		UFolderDialogs::GetFolderAndServerNames(
						inFolder,
						(UFolderDialogs::FolderKind)inKind,
						folderName,
						server);
		cb->SetTitleUsing(folderName, server.GetName());
	}
}

#endif // MOZ_MAIL_NEWS

//-----------------------------------
void UPrefControls::RegisterPrefControlViews()
//-----------------------------------
{
	RegisterClass_(CPrefCheckbox);
	RegisterClass_(CIntPrefRadio);
	RegisterClass_(CBoolPrefRadio);
	RegisterClass_(CPrefTextEdit);
	RegisterClass_(CIntPrefTextEdit);
	RegisterClass_(CIntPrefPopup);
	RegisterClass_(CTextPrefPopup);
	RegisterClass_(CPrefFilePicker);
	RegisterClass_(CPrefColorButton);
#ifdef MOZ_MAIL_NEWS
	RegisterClass_(CSpecialFolderCheckbox);
#endif // MOZ_MAIL_NEWS
}
