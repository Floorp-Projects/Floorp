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

#include "MPreference.h"

#include "PascalString.h"

#include <LControl.h>
#include <LGAPopup.h>

#include "xp_mcom.h"
#include "prefapi.h"
#include "macutil.h" // for StringParamText
#include "prefwutil.h" // for CColorButton

#include "StSetBroadcasting.h"
#include "CTooltipAttachment.h"

Boolean MPreferenceBase::sWriteOnDestroy = false; // must be one for all instantiations of the template.
const char* MPreferenceBase::sReplacementString = nil;

#pragma mark ---CDebugPrefToolTipAttachment---
//========================================================================================
class CDebugPrefToolTipAttachment : public CToolTipAttachment
//========================================================================================
{
	public:
		enum { class_ID = 'X%W@' };

							CDebugPrefToolTipAttachment(MPreferenceBase* b);
	protected:
		virtual void		CalcTipText(
									LWindow*				inOwningWindow,
									LPane*					inOwningPane,
									const EventRecord&		inMacEvent,
									StringPtr				outTipText);
	MPreferenceBase*		mPreferenceBase;
}; // class CDebugPrefToolTipAttachment

//----------------------------------------------------------------------------------------
CDebugPrefToolTipAttachment::CDebugPrefToolTipAttachment(MPreferenceBase* b)
//----------------------------------------------------------------------------------------
	:	CToolTipAttachment(60, 11507)
	,	mPreferenceBase(b)
{
}

//----------------------------------------------------------------------------------------
void CDebugPrefToolTipAttachment::CalcTipText(
//----------------------------------------------------------------------------------------
	LWindow*	/* inOwningWindow */,
	LPane*		/* inOwningPane */,
	const EventRecord&	/* inMacEvent */,
	StringPtr	outTipText)
{
	*(CStr255*)outTipText = mPreferenceBase->mName;
}

#pragma mark ---MPreferenceBase---
//----------------------------------------------------------------------------------------
MPreferenceBase::MPreferenceBase(
	LPane*	inPane
,	LStream* inStream)
//----------------------------------------------------------------------------------------
:	mName(nil)
,	mLocked(false)
,	mPaneSelf(inPane)
{
	CStr255 text;
	inStream->ReadPString(text);
	SetPrefName((const char*)text, false);
	*inStream >> mOrdinal;
} // MPreferenceBase::MPreferenceBase

//----------------------------------------------------------------------------------------
MPreferenceBase::~MPreferenceBase()
//----------------------------------------------------------------------------------------
{
	XP_FREEIF(const_cast<char*>(mName));
} // MPreferenceBase::~MPreferenceBase

//----------------------------------------------------------------------------------------
void MPreferenceBase::SetPrefName(const char* inNewName, Boolean inReread)
//----------------------------------------------------------------------------------------
{
	const char* oldName = mName; // so that inNewName == mName works
	CStr255 text(inNewName);
	if (sReplacementString && *sReplacementString)
		::StringParamText(text, sReplacementString);
	mName = XP_STRDUP((const char*)text);
	XP_FREEIF(const_cast<char*>(oldName));
	if (inReread)
	{
		ReadLockState();
		ReadSelf();
	}
} // MPreferenceBase::ReadLockState

//----------------------------------------------------------------------------------------
void MPreferenceBase::ChangePrefName(LView* inSuperView, PaneIDT inPaneID, const char* inNewName)
//----------------------------------------------------------------------------------------
{
	LPane* p = inSuperView->FindPaneByID(inPaneID);
	MPreferenceBase* pb = dynamic_cast<MPreferenceBase*>(p);
	SignalIf_(!pb);
	if (pb)
		pb->SetPrefName(inNewName);
}

//----------------------------------------------------------------------------------------
const char* MPreferenceBase::GetPrefName(LView* inSuperView, PaneIDT inPaneID)
//----------------------------------------------------------------------------------------
{
	LPane* p = inSuperView->FindPaneByID(inPaneID);
	MPreferenceBase* pb = dynamic_cast<MPreferenceBase*>(p);
	SignalIf_(!pb);
	if (pb)
		return pb->GetPrefName();
	return nil;
}

//----------------------------------------------------------------------------------------
const char* MPreferenceBase::GetPrefName() const
//----------------------------------------------------------------------------------------
{
	return XP_STRDUP(mName);
}

//----------------------------------------------------------------------------------------
void MPreferenceBase::ReadLockState()
//----------------------------------------------------------------------------------------
{
	mLocked = PREF_PrefIsLocked(mName);
	if (mLocked)
		mPaneSelf->Disable();
} // MPreferenceBase::ReadLockState

//----------------------------------------------------------------------------------------
void MPreferenceBase::FinishCreate()
//----------------------------------------------------------------------------------------
{
	ReadLockState();
	ReadSelf();
#ifdef DEBUG
	LAttachable::SetDefaultAttachable(mPaneSelf);
	CDebugPrefToolTipAttachment* a = new CDebugPrefToolTipAttachment(this);
	mPaneSelf->AddAttachment(a);
#endif
} // MPreferenceBase::FinishCreate


//----------------------------------------------------------------------------------------
Boolean MPreferenceBase::ShouldWrite() const
//----------------------------------------------------------------------------------------
{
	if (!sWriteOnDestroy || mLocked)
		return false;
	if (strstr(mName, "^0") != nil) // yow! unreplaced strings
	{
		// Check if a replacement has become possible
		Assert_(sReplacementString && *sReplacementString);
		if (!sReplacementString || !*sReplacementString)
			return false;
		const_cast<MPreferenceBase*>(this)->SetPrefName(mName, false); // don't read
	}
	return true;
		// Note: don't worry about testing Changed(), since preflib does that.
} // MPreferenceBase::ShouldWrite


#pragma mark ---MPreference---
//----------------------------------------------------------------------------------------
template <class TPane, class TData> MPreference<TPane,TData>::MPreference(
						LPane* inPane,
						LStream* inStream)
//----------------------------------------------------------------------------------------
:	MPreferenceBase(inPane, inStream)
{
} // MPreference::MPreference

//----------------------------------------------------------------------------------------
template <class TPane, class TData> MPreference<TPane,TData>::~MPreference()
//----------------------------------------------------------------------------------------
{
} // MPreference::~MPreference

#pragma mark -

enum // what the ordinal means in this case:
{
	kOrdinalXORBit = 1<<0
,	kOrdinalIntBit = 1<<1
};

//----------------------------------------------------------------------------------------
XP_Bool MPreference<LControl,XP_Bool>::GetPaneValue() const
//----------------------------------------------------------------------------------------
{
	return ((LControl*)mPaneSelf)->GetValue();
} // MPreference<LControl,XP_Bool>::GetPaneValue

//----------------------------------------------------------------------------------------
void MPreference<LControl,XP_Bool>::SetPaneValue(XP_Bool inData)
//----------------------------------------------------------------------------------------
{
	((LControl*)mPaneSelf)->SetValue(inData);
} // MPreference<LControl,XP_Bool>::SetPaneValue

//----------------------------------------------------------------------------------------
Boolean MPreference<LControl, XP_Bool>::Changed() const
//----------------------------------------------------------------------------------------
{
	return GetPaneValue() != mInitialControlValue;
} // MPreference<LControl,XP_Bool>::Changed

//----------------------------------------------------------------------------------------
void MPreference<LControl,XP_Bool>::InitializeUsing(PrefReadFunc inFunc)
//----------------------------------------------------------------------------------------
{
	XP_Bool value;
	int	prefResult;
	if (mOrdinal & 2)
	{
		int32 intValue;
		typedef int	(*IntPrefReadFunc)(const char*, int32*);
		prefResult = ((IntPrefReadFunc)inFunc)(mName, &intValue);
		value = intValue;
	}
	else
		prefResult = inFunc(mName, &value);
	if (prefResult == PREF_NOERROR)
		SetPaneValue(value ^ (mOrdinal & kOrdinalXORBit));
} // MPreference<LControl,XP_Bool>::InitializeUsing

//----------------------------------------------------------------------------------------
void MPreference<LControl,XP_Bool>::ReadSelf()
//----------------------------------------------------------------------------------------
{
	if (mOrdinal & kOrdinalIntBit) // this bit indicates it's an int conversion
		InitializeUsing((PrefReadFunc)PREF_GetIntPref);
	else
		InitializeUsing(PREF_GetBoolPref);
	mInitialControlValue = GetPaneValue();
} // MPreference<LControl,XP_Bool>::ReadSelf

//----------------------------------------------------------------------------------------
void MPreference<LControl,XP_Bool>::ReadDefaultSelf()
//----------------------------------------------------------------------------------------
{
	if (!IsLocked())
		if (mOrdinal & kOrdinalIntBit) // this bit indicates it's an int conversion
			InitializeUsing((PrefReadFunc)PREF_GetDefaultIntPref);
		else
			InitializeUsing(PREF_GetDefaultBoolPref);
} // MPreference<LControl,XP_Bool>::ReadDefaultSelf

//----------------------------------------------------------------------------------------
void MPreference<LControl,XP_Bool>::WriteSelf()
//----------------------------------------------------------------------------------------
{
	if (ShouldWrite())
	{
		if (mOrdinal & kOrdinalIntBit) // this bit indicates it's an int conversion
			PREF_SetIntPref(mName, GetPrefValue());
		else
			PREF_SetBoolPref(mName, GetPrefValue());
	}
} // MPreference<LControl,XP_Bool>::WriteSelf

//----------------------------------------------------------------------------------------
XP_Bool MPreference<LControl,XP_Bool>::GetPrefValue() const
//----------------------------------------------------------------------------------------
{
	// xor the boolean value with the low-order bit of the ordinal
	return (XP_Bool)(mPaneSelf->GetValue() ^ (mOrdinal & kOrdinalXORBit));
} // MPreference<LControl,XP_Bool>::GetPrefValue

template class MPreference<LControl,XP_Bool>;

#pragma mark -

//----------------------------------------------------------------------------------------
int32 MPreference<LControl,int32>::GetPaneValue() const
//----------------------------------------------------------------------------------------
{
	return ((LControl*)mPaneSelf)->GetValue();
} // MPreference<LControl,int32>::GetPaneValue

//----------------------------------------------------------------------------------------
void MPreference<LControl,int32>::SetPaneValue(int32 inData)
//----------------------------------------------------------------------------------------
{
	((LControl*)mPaneSelf)->SetValue(inData);
} // MPreference<LControl,int32>::SetPaneValue

//----------------------------------------------------------------------------------------
Boolean MPreference<LControl, int32>::Changed() const
//----------------------------------------------------------------------------------------
{
	return GetPaneValue() != mInitialControlValue;
} // MPreference<LControl,int32>::Changed

//----------------------------------------------------------------------------------------
void MPreference<LControl,int32>::InitializeUsing(PrefReadFunc inFunc)
//----------------------------------------------------------------------------------------
{
	int32 value;
	int	prefResult = inFunc(mName, &value);
	if (prefResult == PREF_NOERROR)
	{
		if (value == mOrdinal)
			SetPaneValue(1); // tab group will turn others off.
	}
} // MPreference<LControl,int32>::InitializeUsing

//----------------------------------------------------------------------------------------
void MPreference<LControl,int32>::ReadSelf()
//----------------------------------------------------------------------------------------
{
	InitializeUsing(PREF_GetIntPref);
	mInitialControlValue = GetPaneValue();
} // MPreference<LControl,int32>::ReadSelf

//----------------------------------------------------------------------------------------
void MPreference<LControl,int32>::ReadDefaultSelf()
//----------------------------------------------------------------------------------------
{
	if (!IsLocked())
		InitializeUsing(PREF_GetDefaultIntPref);
} // MPreference<LControl,int32>::ReadDefaultSelf

//----------------------------------------------------------------------------------------
void MPreference<LControl,int32>::WriteSelf()
//----------------------------------------------------------------------------------------
{
	if (ShouldWrite())
		PREF_SetIntPref(mName, mOrdinal);
} // MPreference<int>::WriteSelf

//----------------------------------------------------------------------------------------
int32 MPreference<LControl,int32>::GetPrefValue() const
//----------------------------------------------------------------------------------------
{
	return mOrdinal;
} // MPreference<int>::GetPrefValue

template class MPreference<LControl,int32>;

#pragma mark -

//----------------------------------------------------------------------------------------
MPreference<LTextEdit,char*>::~MPreference()
//----------------------------------------------------------------------------------------
{
	XP_FREEIF(mInitialControlValue);
} // MPreference<LTextEdit,char*>::CleanUpData

//----------------------------------------------------------------------------------------
char* MPreference<LTextEdit,char*>::GetPaneValue() const
//----------------------------------------------------------------------------------------
{
	CStr255 value;
	mPaneSelf->GetDescriptor(value);
	return (char*)value;
} // MPreference<LTextEdit,char*>::GetPaneValue

//----------------------------------------------------------------------------------------
void MPreference<LTextEdit,char*>::SetPaneValue(char* inData)
//----------------------------------------------------------------------------------------
{
	((LTextEdit*)mPaneSelf)->SetDescriptor(CStr255(inData));
} // MPreference<LTextEdit,char*>:SetPaneValue

//----------------------------------------------------------------------------------------
Boolean MPreference<LTextEdit,char*>::Changed() const
//----------------------------------------------------------------------------------------
{
	char* value = GetPaneValue();
	if (value && *value)
		return (strcmp(value, mInitialControlValue) != 0);
	return true;
} // MPreference<LTextEdit,char*>::Changed

//----------------------------------------------------------------------------------------
void MPreference<LTextEdit,char*>::InitializeUsing(PrefReadFunc inFunc)
//----------------------------------------------------------------------------------------
{
	char* value;
	int	prefResult = inFunc(mName, &value);
	if (prefResult == PREF_NOERROR)
	{
		SetPaneValue(value);
		XP_FREEIF(value);
	}
} // MPreference<LTextEdit,char*>::InitializeUsing

//----------------------------------------------------------------------------------------
void MPreference<LTextEdit,char*>::ReadSelf()
//----------------------------------------------------------------------------------------
{
	InitializeUsing(PREF_CopyCharPref);
	mInitialControlValue = XP_STRDUP(GetPaneValue());
} // MPreference<LTextEdit,char*>::ReadSelf

//----------------------------------------------------------------------------------------
void MPreference<LTextEdit,char*>::ReadDefaultSelf()
//----------------------------------------------------------------------------------------
{
	if (!IsLocked())
		InitializeUsing(PREF_CopyDefaultCharPref);
} // MPreference<LTextEdit,char*>::ReadDefaultSelf

//----------------------------------------------------------------------------------------
void MPreference<LTextEdit,char*>::WriteSelf()
//----------------------------------------------------------------------------------------
{
	if (ShouldWrite())
		PREF_SetCharPref(mName, GetPaneValue());
} // MPreference<LTextEdit,char*>::WriteSelf

//----------------------------------------------------------------------------------------
char* MPreference<LTextEdit,char*>::GetPrefValue() const
//----------------------------------------------------------------------------------------
{
	return GetPaneValue();
} // MPreference<LTextEdit,char*>::GetPrefValue

template class MPreference<LTextEdit,char*>;

#pragma mark -

// This is used for captions, and for mixing in with another pref control (eg, to
// control the descriptor of a checkbox).

//----------------------------------------------------------------------------------------
MPreference<LPane,char*>::~MPreference()
//----------------------------------------------------------------------------------------
{
	XP_FREEIF(mInitialControlValue);
} // MPreference<LPane,char*>::CleanUpData

//----------------------------------------------------------------------------------------
char* MPreference<LPane,char*>::GetPaneValue() const
//----------------------------------------------------------------------------------------
{
	CStr255 value;
	mPaneSelf->GetDescriptor(value);
	return (char*)value;
} // MPreference<LPane,char*>::GetPaneValue

//----------------------------------------------------------------------------------------
void MPreference<LPane,char*>::SetPaneValue(char* inData)
//----------------------------------------------------------------------------------------
{
	((LPane*)mPaneSelf)->SetDescriptor(CStr255(inData));
} // MPreference<LPane,char*>:SetPaneValue

//----------------------------------------------------------------------------------------
Boolean MPreference<LPane,char*>::Changed() const
//----------------------------------------------------------------------------------------
{
	char* value = GetPaneValue();
	if (value && *value)
		return (strcmp(value, mInitialControlValue) != 0);
	return true;
} // MPreference<LPane,char*>::Changed

//----------------------------------------------------------------------------------------
void MPreference<LPane,char*>::InitializeUsing(PrefReadFunc inFunc)
//----------------------------------------------------------------------------------------
{
	char* value;
	int	prefResult = inFunc(mName, &value);
	if (prefResult == PREF_NOERROR)
	{
		SetPaneValue(value);
		XP_FREEIF(value);
	}
} // MPreference<LPane,char*>::InitializeUsing

//----------------------------------------------------------------------------------------
void MPreference<LPane,char*>::ReadSelf()
//----------------------------------------------------------------------------------------
{
	InitializeUsing(PREF_CopyCharPref);
	mInitialControlValue = XP_STRDUP(GetPaneValue());
} // MPreference<LPane,char*>::ReadSelf

//----------------------------------------------------------------------------------------
void MPreference<LPane,char*>::ReadDefaultSelf()
//----------------------------------------------------------------------------------------
{
	if (!IsLocked())
		InitializeUsing(PREF_CopyDefaultCharPref);
} // MPreference<LPane,char*>::ReadDefaultSelf

//----------------------------------------------------------------------------------------
void MPreference<LPane,char*>::WriteSelf()
//----------------------------------------------------------------------------------------
{
} // MPreference<LPane,char*>::WriteSelf

//----------------------------------------------------------------------------------------
char* MPreference<LPane,char*>::GetPrefValue() const
//----------------------------------------------------------------------------------------
{
	return GetPaneValue();
} // MPreference<LPane,char*>::GetPrefValue

template class MPreference<LPane,char*>;

#pragma mark -

//----------------------------------------------------------------------------------------
MPreference<LGAPopup,char*>::~MPreference()
//----------------------------------------------------------------------------------------
{
	XP_FREEIF(mInitialControlValue);
} // MPreference<LGAPopup,char*>::CleanUpData

//----------------------------------------------------------------------------------------
char* MPreference<LGAPopup,char*>::GetPaneValue() const
//----------------------------------------------------------------------------------------
{
	Int32 itemNumber = ((LGAPopup*)mPaneSelf)->GetValue();
	CStr255 itemString;
	if (itemNumber > 0)
	{
		MenuHandle menuH = ((LGAPopup*)mPaneSelf)->GetMacMenuH();
		::GetMenuItemText(menuH, itemNumber, itemString);
	}
	return (char*)itemString;
} // MPreference<LGAPopup,char*>::GetPaneValue

//----------------------------------------------------------------------------------------
void MPreference<LGAPopup,char*>::SetPaneValue(char* inData)
//----------------------------------------------------------------------------------------
{
	if (!inData || !*inData)
		return;
	MenuHandle menuH = ((LGAPopup*)mPaneSelf)->GetMacMenuH();
	short menuSize = ::CountMItems(menuH);
	CStr255	itemString;
	for (short menuItem = 1; menuItem <= menuSize; ++menuItem)
	{
		::GetMenuItemText(menuH, menuItem, itemString);
		if (itemString == inData)
		{
			StSetBroadcasting dontBroadcast((LGAPopup*)mPaneSelf, false);
			((LGAPopup*)mPaneSelf)->SetValue(menuItem);
			return;
		}
	}
} // MPreference<LGAPopup,char*>:SetPaneValue

//----------------------------------------------------------------------------------------
Boolean MPreference<LGAPopup,char*>::Changed() const
//----------------------------------------------------------------------------------------
{
	char* value = GetPaneValue();
	if (value && *value)
		return (strcmp(value, mInitialControlValue) != 0);
	return true;
} // MPreference<LGAPopup,char*>::Changed

//----------------------------------------------------------------------------------------
void MPreference<LGAPopup,char*>::InitializeUsing(PrefReadFunc inFunc)
//----------------------------------------------------------------------------------------
{
	char* value;
	int	prefResult = inFunc(mName, &value);
	if (prefResult == PREF_NOERROR)
	{
		SetPaneValue(value);
		XP_FREEIF(value);
	}
} // MPreference<LGAPopup,char*>::InitializeUsing

//----------------------------------------------------------------------------------------
void MPreference<LGAPopup,char*>::ReadSelf()
//----------------------------------------------------------------------------------------
{
	InitializeUsing(PREF_CopyCharPref);
	mInitialControlValue = XP_STRDUP(GetPaneValue());
} // MPreference<LGAPopup,char*>::ReadSelf

//----------------------------------------------------------------------------------------
void MPreference<LGAPopup,char*>::ReadDefaultSelf()
//----------------------------------------------------------------------------------------
{
	if (!IsLocked())
		InitializeUsing(PREF_CopyDefaultCharPref);
} // MPreference<LGAPopup,char*>::ReadDefaultSelf

//----------------------------------------------------------------------------------------
void MPreference<LGAPopup,char*>::WriteSelf()
//----------------------------------------------------------------------------------------
{
	if (ShouldWrite())
		PREF_SetCharPref(mName, GetPaneValue());
} // MPreference<LGAPopup,char*>::WriteSelf

//----------------------------------------------------------------------------------------
char* MPreference<LGAPopup,char*>::GetPrefValue() const
//----------------------------------------------------------------------------------------
{
	return GetPaneValue();
} // MPreference<LGAPopup,char*>::GetPrefValue

template class MPreference<LGAPopup,char*>;

#pragma mark -

//----------------------------------------------------------------------------------------
MPreference<LTextEdit,int32>::~MPreference()
//----------------------------------------------------------------------------------------
{
} // MPreference<LTextEdit,int32>::CleanUpData

//----------------------------------------------------------------------------------------
int32 MPreference<LTextEdit,int32>::GetPaneValue() const
//----------------------------------------------------------------------------------------
{
	return ((LTextEdit*)mPaneSelf)->GetValue();
} // MPreference<LTextEdit,int32>::GetPaneValue

//----------------------------------------------------------------------------------------
void MPreference<LTextEdit,int32>::SetPaneValue(int32 inData)
//----------------------------------------------------------------------------------------
{
	((LTextEdit*)mPaneSelf)->SetValue(inData);
} // MPreference<LTextEdit,int32>:SetPaneValue

//----------------------------------------------------------------------------------------
Boolean MPreference<LTextEdit,int32>::Changed() const
//----------------------------------------------------------------------------------------
{
	return GetPaneValue() != mInitialControlValue;
} // MPreference<LTextEdit,int32>::Changed

//----------------------------------------------------------------------------------------
void MPreference<LTextEdit,int32>::InitializeUsing(PrefReadFunc inFunc)
//----------------------------------------------------------------------------------------
{
	int32 value;
	int	prefResult = inFunc(mName, &value);
	if (prefResult == PREF_NOERROR)
		SetPaneValue(value);
} // MPreference<LTextEdit,int32>::InitializeUsing

//----------------------------------------------------------------------------------------
void MPreference<LTextEdit,int32>::ReadSelf()
//----------------------------------------------------------------------------------------
{
	InitializeUsing(PREF_GetIntPref);
	mInitialControlValue = GetPaneValue();
} // MPreference<LTextEdit,int32>::ReadSelf

//----------------------------------------------------------------------------------------
void MPreference<LTextEdit,int32>::ReadDefaultSelf()
//----------------------------------------------------------------------------------------
{
	if (!IsLocked())
		InitializeUsing(PREF_GetDefaultIntPref);
} // MPreference<LTextEdit,int32>::ReadDefaultSelf

//----------------------------------------------------------------------------------------
void MPreference<LTextEdit,int32>::WriteSelf()
//----------------------------------------------------------------------------------------
{
	if (ShouldWrite())
		PREF_SetIntPref(mName, GetPaneValue());
} // MPreference<LTextEdit,int32>::WriteSelf

//----------------------------------------------------------------------------------------
int32 MPreference<LTextEdit,int32>::GetPrefValue() const
//----------------------------------------------------------------------------------------
{
	return GetPaneValue();
} // MPreference<LTextEdit,int32>::GetPrefValue

template class MPreference<LTextEdit,int32>;

#pragma mark -

// The function signature for reading colors is not like the other types.  Here is
// its prototype, which we use to cast back and forth.
typedef int (*ColorReadFunc)(const char*, uint8*, uint8*, uint8*);

//----------------------------------------------------------------------------------------
MPreference<CColorButton,RGBColor>::~MPreference()
//----------------------------------------------------------------------------------------
{
} // MPreference<CColorButton,RGBColor>::CleanUpData

//----------------------------------------------------------------------------------------
RGBColor MPreference<CColorButton,RGBColor>::GetPaneValue() const
//----------------------------------------------------------------------------------------
{
	return ((CColorButton*)mPaneSelf)->GetColor();
} // MPreference<CColorButton,RGBColor>::GetPaneValue

//----------------------------------------------------------------------------------------
void MPreference<CColorButton,RGBColor>::SetPaneValue(RGBColor inData)
//----------------------------------------------------------------------------------------
{
	((CColorButton*)mPaneSelf)->SetColor(inData);
	mPaneSelf->Refresh();
} // MPreference<CColorButton,RGBColor>:SetPaneValue

//----------------------------------------------------------------------------------------
Boolean MPreference<CColorButton,RGBColor>::Changed() const
//----------------------------------------------------------------------------------------
{
	return memcmp(&GetPaneValue(), &mInitialControlValue, sizeof(RGBColor)) == 0;
} // MPreference<CColorButton,RGBColor>::Changed

//----------------------------------------------------------------------------------------
void MPreference<CColorButton,RGBColor>::InitializeUsing(PrefReadFunc inFunc)
//----------------------------------------------------------------------------------------
{
	RGBColor value;
	uint8 red = 0, green = 0, blue = 0;
	int	prefResult = ((ColorReadFunc)inFunc)(mName, &red, &green, &blue);
	if (prefResult == PREF_NOERROR)
	{
		value.red = red << 8;
		value.green = green << 8;
		value.blue = blue << 8;
		SetPaneValue(value);
	}
} // MPreference<CColorButton,RGBColor>::InitializeUsing

//----------------------------------------------------------------------------------------
void MPreference<CColorButton,RGBColor>::ReadSelf()
//----------------------------------------------------------------------------------------
{
	InitializeUsing((PrefReadFunc)PREF_GetColorPref);
	mInitialControlValue = GetPaneValue();
} // MPreference<CColorButton,RGBColor>::ReadSelf

//----------------------------------------------------------------------------------------
void MPreference<CColorButton,RGBColor>::ReadDefaultSelf()
//----------------------------------------------------------------------------------------
{
	if (!IsLocked())
		InitializeUsing((PrefReadFunc)PREF_GetDefaultColorPref);
} // MPreference<CColorButton,RGBColor>::ReadDefaultSelf

//----------------------------------------------------------------------------------------
void MPreference<CColorButton,RGBColor>::WriteSelf()
//----------------------------------------------------------------------------------------
{
	if (ShouldWrite())
	{
		RGBColor value = GetPaneValue();
		PREF_SetColorPref(mName, value.red >> 8, value.green >> 8, value.blue >> 8);
	}
} // MPreference<CColorButton,RGBColor>::WriteSelf

//----------------------------------------------------------------------------------------
RGBColor MPreference<CColorButton,RGBColor>::GetPrefValue() const
//----------------------------------------------------------------------------------------
{
	return GetPaneValue();
} // MPreference<CColorButton,RGBColor>::GetPrefValue

template class MPreference<CColorButton,RGBColor>;

