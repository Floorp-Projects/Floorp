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

#include <LGACheckbox.h>
#include <LGARadioButton.h>
#include <LGAPopup.h>
#include <LGACaption.h>

// Note.	We can do java-style 'inline' virtual functions here, because these classes
//			all have file scope.  If you ever move these declarations to a header, you
//			should non-inline 'em.

#pragma mark ---CPrefCheckbox---
//======================================
class CPrefCheckbox
//======================================
:	public LGACheckbox
,	public MPreference<LControl,XP_Bool>
{
public:
	CPrefCheckbox(LStream* inStream)
		:	LGACheckbox(inStream)
		,	MPreference<LControl,XP_Bool>(this, inStream)
		{
		}
	virtual ~CPrefCheckbox()
		{
			WriteSelf();
				// checkbox always writes its pane value ^ its ordinal.
			 	// Thus: if mOrdinal == 1, the checkbox reverses the sense of the pref.
		}
	enum { class_ID = 'Pchk' };
	virtual void FinishCreateSelf()
		{
			LGACheckbox::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
		}
}; // class CPrefCheckbox

#pragma mark ---CBoolPrefRadio---
//======================================
class CBoolPrefRadio
// Actually, because of the writing-out policy, only the zero-ordinal radio button
// needs to be a CBoolPrefRadio.  The other one can be just a LGARadioButton.
//======================================
:	public LGARadioButton
,	public MPreference<LControl,XP_Bool>
{
public:
	CBoolPrefRadio(LStream* inStream)
		:	LGARadioButton(inStream)
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
	enum { class_ID = 'Brad' };
	virtual void FinishCreateSelf()
		{
			LGARadioButton::FinishCreateSelf();
			MPreferenceBase::FinishCreate();
		}
}; // class CBoolPrefRadio

#pragma mark ---CIntPrefRadio---
//======================================
class CIntPrefRadio
//======================================
:	public LGARadioButton
,	public MPreference<LControl,int32>
{
public:
	CIntPrefRadio(LStream* inStream)
		:	LGARadioButton(inStream)
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
	enum { class_ID = 'Irad' };
	virtual void FinishCreateSelf()
		{
			LGARadioButton::FinishCreateSelf();
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
	virtual int32 GetPrefValue() const
		{
			return GetPaneValue();
		}
	virtual void InitializeUsing(PrefReadFunc inFunc)
		{
			int32 value;
			int	prefResult = inFunc(GetValidPrefName(), &value);
			if (prefResult == PREF_NOERROR)
				SetPaneValue(value);
		}
	virtual void ReadSelf()
		{
			InitializeUsing(PREF_GetIntPref);
		}
	virtual void ReadDefaultSelf()
		{
			if (!IsLocked())
				InitializeUsing(PREF_GetDefaultIntPref);
		}
	virtual void WriteSelf()
		{
			if (ShouldWrite())
				PREF_SetIntPref(GetPrefName(), GetPaneValue());
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
	enum { class_ID = 'Pedt' };
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
	enum { class_ID = 'Iedt' };
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
						PREF_SetBinaryPref(GetPrefName(), *a, iByteCnt);
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
			if (inFunc(GetValidPrefName(), &a, &size ) == 0)
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

#pragma mark ---MSpecialFolderMixin---
//======================================
class MSpecialFolderMixin
// Like a pref checkbox, but the descriptor is derived from a second, text preference.
// The second pref name is obtained from the main (boolean) one by replacing the substring
// "use" by the substring "default", eg
//		mail.use_fcc -> mail.default_fcc
// This conversion allows us to share the constructor resource template.  This guy can just have
// a different class ID.
//======================================
{
public:
					MSpecialFolderMixin(LPane* inPane, const char* inPrefName);
	virtual			~MSpecialFolderMixin();
	void			FinishCreateSelf();
	void			SetTitleUsing(const CStr255& folderName, const CStr255& serverName);
	const char*		GetCaptionPrefName() const
					{
						return mCaptionPrefName;
					}
	void			SetCaptionPrefName(const char* inCaptionPrefName)
					{
						XP_FREEIF(mCaptionPrefName);
						mCaptionPrefName = XP_STRDUP(inCaptionPrefName);
					}

protected:
	LPane*			mPane;
	char*			mCaptionPrefName;
	char*			mFormatString;
}; // class MSpecialFolderMixin

//-----------------------------------
MSpecialFolderMixin::MSpecialFolderMixin(LPane* inPane, const char* inPrefName)
//-----------------------------------
:	mPane(inPane)
,	mCaptionPrefName(nil)
,	mFormatString(nil)
{
	LStr255 originalName = inPrefName;
	UInt8 position = originalName.Find(".use_", 1);
	originalName.Replace(position, strlen(".use_"), ".default_", strlen(".default_"));
	SetCaptionPrefName((const char*)CStr255(ConstStringPtr(originalName)));
	// The initial descriptor is the format string.
	CStr255 text;
	mPane->GetDescriptor(text);
	mFormatString = XP_STRDUP((char*)text);
}

//-----------------------------------
MSpecialFolderMixin::~MSpecialFolderMixin()
//-----------------------------------
{
	XP_FREEIF(mCaptionPrefName);
	XP_FREEIF(mFormatString);
}

//-----------------------------------
void MSpecialFolderMixin::FinishCreateSelf()
//-----------------------------------
{
	CStr255 folderName;
	CMessageFolder folder, server;
	UFolderDialogs::GetFolderAndServerNames(
					mCaptionPrefName,
					folder,
					folderName,
					server);
	
	SetTitleUsing(folderName, server.GetName());
}

//-----------------------------------
void MSpecialFolderMixin::SetTitleUsing(
	const CStr255& folderName,
	const CStr255& serverName)
//-----------------------------------
{
	CStr255 title(mFormatString);
	StringParamText(title, folderName, serverName);
	mPane->SetDescriptor(title);
}

#pragma mark ---CSpecialFolderCheckbox---
//======================================
class CSpecialFolderCheckbox
// Like a pref checkbox, but the descriptor is derived from a second, text preference.
// The second pref name is obtained from the main (boolean) one by replacing the substring
// "use" by the substring "default", eg
//		mail.use_fcc -> mail.default_fcc
// This conversion allows us to share the constructor resource template.  This guy can just have
// a different class ID.
//======================================
:	public CPrefCheckbox
,	public MSpecialFolderMixin
{
private:
	typedef CPrefCheckbox Inherited;
public:
	enum { class_ID = 'FLck' };
					CSpecialFolderCheckbox(LStream* inStream);
	virtual void	FinishCreateSelf();
}; // class CSpecialFolderCheckbox

//-----------------------------------
CSpecialFolderCheckbox::CSpecialFolderCheckbox(LStream* inStream)
//-----------------------------------
:	CPrefCheckbox(inStream)
,	MSpecialFolderMixin((LPane*)this, GetValidPrefName())
{
}

//-----------------------------------
void CSpecialFolderCheckbox::FinishCreateSelf()
//-----------------------------------
{
	Inherited::FinishCreateSelf();
	MSpecialFolderMixin::FinishCreateSelf();
}

#pragma mark ---CSpecialFolderCaption---
//======================================
class CSpecialFolderCaption
// See comments for CSpecialFolderCheckbox.  This differs only in that there is no checkbox
// there.
//======================================
:	public LGACaption
,	public MPreference<LGACaption,XP_Bool>
,	public MSpecialFolderMixin
{
private:
	typedef LGACaption Inherited;
public:
	enum { class_ID = 'FLcp' };
					CSpecialFolderCaption(LStream* inStream);
	virtual void	FinishCreateSelf();
}; // class CSpecialFolderCaption

//-----------------------------------
CSpecialFolderCaption::CSpecialFolderCaption(LStream* inStream)
//-----------------------------------
:	LGACaption(inStream)
,	MPreference<LGACaption, XP_Bool>(this, inStream)
,	MSpecialFolderMixin((LPane*)this, GetValidPrefName())
{
}

//-----------------------------------
void CSpecialFolderCaption::FinishCreateSelf()
//-----------------------------------
{
	Inherited::FinishCreateSelf();
	MPreferenceBase::FinishCreate();
	MSpecialFolderMixin::FinishCreateSelf();
}

//-----------------------------------
void UPrefControls::NoteSpecialFolderChanged(
	LPane* inDescriptionPane,
	int inKind,
	const CMessageFolder& inFolder)
// The control is a checkbox displaying (e.g.) "Sent Mail on FooServer". This routine updates
// the title after the choice of folder or server has changed.
//-----------------------------------
{
	MSpecialFolderMixin* cb = dynamic_cast<MSpecialFolderMixin*>(inDescriptionPane);
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

//-----------------------------------
void UPrefControls::NoteSpecialFolderChanged(
	LPane* inDescriptionPane,
	const char* inNewCaptionPrefName)
// The control is a checkbox displaying (e.g.) "Sent Mail on FooServer". This routine updates
// the title after the choice of folder or server has changed.
//-----------------------------------
{
	MSpecialFolderMixin* cb = dynamic_cast<MSpecialFolderMixin*>(inDescriptionPane);
	if (cb && XP_STRCMP(cb->GetCaptionPrefName(), inNewCaptionPrefName) != 0)
	{
		cb->SetCaptionPrefName(inNewCaptionPrefName);
		CStr255 folderName;
		CMessageFolder folder, server;
		UFolderDialogs::GetFolderAndServerNames(
						inNewCaptionPrefName,
						folder,
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
	RegisterClass_(CSpecialFolderCaption);
#endif // MOZ_MAIL_NEWS
}
