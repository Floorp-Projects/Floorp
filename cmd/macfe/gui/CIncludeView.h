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
#include "LPane.h"

//======================================
class CIncludeView : public LPane
// This pane lets its superview "include" another view, allowing several windows to
// share a subview hierarchy that is defined by single 'PPob' resource.
// When FinishCreateSelf() is called, this view creates the installed view, and
// then replaces itself by that view.  All location characteristics of the original view
// are copied into the newly installed view.
//======================================
{

	typedef LPane Inherited;


public:
	enum { class_ID = 'Incl' };
	CIncludeView(LStream* inStream);
	virtual ~CIncludeView();
	virtual void FinishCreateSelf();
protected:
	virtual void InstallPane(LPane* inPane);

// data
protected:
	ResIDT mInstalledViewID;
}; // class LIncludeView
