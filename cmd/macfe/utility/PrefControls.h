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

#ifdef MOZ_MAIL_NEWS
class CMessageFolder;
#endif // MOZ_MAIL_NEWS

//======================================
class UPrefControls
// The control classes themselves are all private.  That's the point.  They know what
// they're doing.

// So far, the family includes
// CPrefTextEdit, CPrefCheckbox, CIntPrefRadio, and CBoolPrefRadio.  Each of these 
// has a custom constructor template. 
//
// These can be added to any window (not just the preferences dialog).  You add the 
// control to your window in constructor, put in the pref name (eg 
// "browser.foo.bar.mumble") and a number called "mOrdinal". 
//
// In most cases, no code will be required.  The control initializes itself by querying 
// the pref value during FinishCreate(), and updates the associated preference when 
// destroyed.  The latter behavior is determined by a static function call that you 
// should make if the user hits a "cancel" button. 
//
// The meaning of mOrdinal varies with the type of pane.  For a checkbox, mOrdinal 
// is xored with the value when converting between the pref and the control value, 
// allowing a checkbox to be worded in the same or the opposite sense to the pref's
// semantic.  For a boolean radio button group, the one with mOrdinal == 0 corresponds
// to the pref value and updates its corresponding pref, while the other one must have 
// mOrdinal == 1.  For an int radio button group, the one whose ordinal matches the 
// value of the pref gets turned on initially and on destruction, only the one that's 
// on updates its corresponding pref to the value of its  mOrdinal field. 
//
// Future directions might include a popup menu control associated with an int 
// value, an RGAColor pref control, etc. 
//======================================
{
public:
	static void		RegisterPrefControlViews();
#ifdef MOZ_MAIL_NEWS
	static void		NoteSpecialFolderChanged(
								LControl* inControl,
								int inKind, /* actually UFolderDialogs::FolderKind */
								const CMessageFolder& inFolder);
#endif // MOZ_MAIL_NEWS
};