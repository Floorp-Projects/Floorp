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

//----------------------------------------------------------------------------------------
//	MPreferenceBase is the base class for all the prefs controls and edit fields
//	in the prefs dialog, and properties dialogs. It allows preference panels to be
//	easily extended with additional pref controls with just Constructor changes,
//	as the controls know which pref they represent, and initialize themselves and
//	write themselves out to the prefs accordingly.
//	
//	There are two additional mechanisms that are used in sub-dialogs of the preferences
//	dialog, to permit the following:
//	
//	1. 	Sub-dialogs can control the pref for a given server (e.g. IMAP server).
//	
//		This is achivieved by a ParamText-type mechanism. Controls in these sub-dialogs
//		contain pref strings like: "mail.imap.server.^0.username". These controls 
//		construct the real prefs string ("mail.imap.server.nsmail-1.username") on the
//		fly when reading and writing prefs, e.g. in MPreferenceBase::GetPrefName().
//	
//		The stack-based class StReplacementString() allows you to set this replacement
//		string temporarily, e.g. while showing properties for a mail server.
//		
//	2. 	Sub-dialogs can write their preferences out to a temporary sub-tree of the
//		main preferences, the values in which are then copied over to the real
//		prefs at some later time (e.g. when the user OKs the main prefs dialog
//	
//		This is achived by prepending a prefix to the prefs name that is stored
//		in the PPob. Controls then attempt to read their values first from the
//		prefs cache (with the prefix), then from the main prefs (without the prefix)
//		[see GetValidPrefName()]. They then write their values to the temporary prefs
//		cache if the prefix is in force when the dialog is destroyed.
//
//		The stack-based class StUseTempPrefCache enables this functionality; typically
//		you'd create one in scope around a call to conduct the sub-dialog , since
//		the prefix string needs to be in place both when the controls are constructed
//		and destroyed.
//
//		To copy these cached preference values over to the main prefs tree, call 
//		CopyCachedPrefsToMainPrefs(), say when the user OKs the main prefs dialog.
//	
//----------------------------------------------------------------------------------------

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
	virtual const char*	GetPrefName();
	virtual const char*	GetValidPrefName();
	virtual const char*	GetUnsubstitutedPrefName();
	virtual void		SetWritePref(Boolean inWritePref) { mWritePref = inWritePref; }
	virtual Boolean		GetWritePref() const { return mWritePref; }
	
	static void			ChangePrefName(LView* inSuperView, PaneIDT inPaneID, const char* inNewName);
	static const char*	GetPanePrefName(LView* inSuperView, PaneIDT inPaneID);
	static const char*	GetPaneUnsubstitutedPrefName(LView* inSuperView, PaneIDT inPaneID);
	
	static void			SetWriteOnDestroy(Boolean inWrite) { sWriteOnDestroy = inWrite; }
	static Boolean		GetWriteOnDestroy() { return sWriteOnDestroy; }
	
	static void			SetPaneWritePref(LView* inSuperView, PaneIDT inPaneID, Boolean inWritePref);
	
	static void			InitTempPrefCache();
	static void			SetUseTempPrefCache(Boolean inUseTempPrefPrefix) { sUseTempPrefPrefix = inUseTempPrefPrefix ; }
	static Boolean		GetUseTempPrefCache() { return sUseTempPrefPrefix; }
	
	static void			CopyCachedPrefsToMainPrefs();
	static void			SetReplacementString(const char* s);
	
	struct StReplacementString // temporarily set and then clear the static string
	{
		StReplacementString(const char* s)
			{ 
				MPreferenceBase::SetReplacementString(s);
			}
		~StReplacementString()
			{
				MPreferenceBase::SetReplacementString(nil);
			}
	};
	
	struct StUseTempPrefCache // temporarily set and then restore the static flag
	{
		StUseTempPrefCache(Boolean inTempValue)
			:	mSavedValue(MPreferenceBase::sUseTempPrefPrefix)
			{
				MPreferenceBase::sUseTempPrefPrefix = inTempValue;
			}
		~StUseTempPrefCache()
			{
				MPreferenceBase::sUseTempPrefPrefix = mSavedValue;
			}
		Boolean mSavedValue;
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
	Int16				mOrdinal;
		// For a boolean pref, this is xored with the value after read and before write.
		// For an int pref, this represents the value to be saved if the control is on.
	// From the prefs db
	Boolean				mLocked;
	Boolean				mWritePref;		// set to false to tell the control not to write out the pref
	LPane*				mPaneSelf; 		// Control/edit field that we're mixed in with.

	const char*			mSubstitutedName;	// name after substitution. Freed on destroy.
private:
	const char*			mName;		// the prefs name format string
	
	// Implementation
private:
	friend class		CDebugPrefToolTipAttachment;
	friend class		StReplacementString;
	friend class		StWriteOnDestroy;
	friend class 		StUseTempPrefCache;
	static Boolean		sWriteOnDestroy; 		// one for all instantiations of the template.
	static Boolean		sUseTempPrefPrefix; 	// one for all instantiations of the template.
	static char*		sReplacementString; 	// for pref name magic names with ^0 in them.
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
	virtual void	WriteSelf();
	virtual TData	GetPrefValue() const;
	virtual TData	GetPaneValue() const;
	virtual void	SetPaneValue(TData);
	virtual Boolean	Changed() const;
protected:
	typedef int		(*PrefReadFunc)(const char*, TData*);
	virtual Boolean	ShouldWrite() const { return MPreferenceBase::ShouldWrite(); }
	virtual void	ReadLockState() { MPreferenceBase::ReadLockState(); }
	virtual void	InitializeUsing(PrefReadFunc inFunc); // used by ReadSelf, ReadDefaultSelf.
// data:
protected:
	// From the constructor:
	TData			mInitialControlValue;
}; // template <class TPane, class TData> class MPreference

