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

#pragma once

class LControl;
class LStream;

//======================================
class MPreferenceBase
//======================================
{
protected:
					
						MPreferenceBase(
							LPane* inPane,
							LStream* inStream);
	virtual				~MPreferenceBase();
public:
	Boolean				IsLocked() const { return mLocked; }
	virtual void		ReadDefaultSelf() = 0; // Reset factory default
	virtual Boolean		Changed() const = 0;
	virtual void		SetPrefName(const char* inNewName, Boolean inReread = true);
	virtual const char*	GetPrefName() const; // returns an allocated string.
	
	static void			ChangePrefName(LView* inSuperView, PaneIDT inPaneID, const char* inNewName);
	static const char*	GetPrefName(LView* inSuperView, PaneIDT inPaneID); // returns an allocated string.
	static void			SetWriteOnDestroy(Boolean inWrite) { sWriteOnDestroy = inWrite; }
	static Boolean		GetWriteOnDestroy() { return sWriteOnDestroy; }
	static void			SetReplacementString(const char* s) { sReplacementString = s; }
	struct StReplacementString // temporarily set and then clear the static string
	{
		StReplacementString(const char* s) { MPreferenceBase::sReplacementString = s; }
		~StReplacementString() { MPreferenceBase::sReplacementString = nil; }
	};
	struct StWriteOnDestroy // temporarily set and then restore the static flag
	{
		StWriteOnDestroy(Boolean inTempValue)
			:	mSavedValue(MPreferenceBase::sWriteOnDestroy)
			{
				MPreferenceBase::sWriteOnDestroy = inTempValue;
			}
		~StWriteOnDestroy()
			{
				MPreferenceBase::sWriteOnDestroy = mSavedValue;
			}
		Boolean mSavedValue;
	};

protected:
	void				FinishCreate();
	virtual void		ReadSelf() = 0; // Call this from FinishCreateSelf!
	Boolean				ShouldWrite() const;
	void				ReadLockState();
// data:
protected:
	// From the resource stream:
	const char*			mName;		// string that identifies this resource.
	Int16				mOrdinal;
		// For a boolean pref, this is xored with the value after read and before write.
		// For an int pref, this represents the value to be saved if the control is on.
	// From the prefs db
	Boolean				mLocked;
	LPane*				mPaneSelf; // Control/edit field that we're mixed in with.
	
	// Implementation
private:
	friend class		CDebugPrefToolTipAttachment;
	friend class		StReplacementString;
	friend class		StWriteOnDestroy;
	static Boolean		sWriteOnDestroy; // one for all instantiations of the template.
	static const char*	sReplacementString; // for pref name magic names with ^0 in them.
		// This must only be manipulated using the StReplacementString class.
}; // class MPreferenceBase


//======================================
template <class TPane,class TData> class MPreference
// This is a mixin class that gets added to the pref control types.
//======================================
:	public MPreferenceBase
{
public:
					MPreference(
						LPane* inControl,
						LStream* inStream);
					virtual ~MPreference();
public:
	virtual void	ReadSelf(); // Call this from FinishCreateSelf!
	virtual void	ReadDefaultSelf();
	void			WriteSelf();
	TData			GetPrefValue() const;
	TData			GetPaneValue() const;
	void			SetPaneValue(TData);
	virtual Boolean	Changed() const;
protected:
	typedef int		(*PrefReadFunc)(const char*, TData*);
	virtual Boolean	ShouldWrite() const { return MPreferenceBase::ShouldWrite(); }
	virtual void	ReadLockState() { MPreferenceBase::ReadLockState(); }
	void			InitializeUsing(PrefReadFunc inFunc); // used by ReadSelf, ReadDefaultSelf.
// data:
protected:
	// From the constructor:
	TData			mInitialControlValue;
}; // template <class TPane, class TData> class MPreference

