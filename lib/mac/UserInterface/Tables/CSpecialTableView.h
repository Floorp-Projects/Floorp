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

#include <LTableView.h>

//======================================
class CSpecialTableView : public LTableView
// If you want your table view to work with a CTableKeyAttachment, inherit from this class.
// This adds the API that CTableKeyAttachment expects, for opening a selection.
//======================================
{
public:
	CSpecialTableView(LStream *inStream) : LTableView(inStream) {}
	virtual void OpenSelection() = 0;
	virtual void SynchronizeColumnsWithHeader() = 0;
	virtual void SetNotifyOnSelectionChange(Boolean /*inDoNotify*/) {}
	virtual Boolean GetNotifyOnSelectionChange() = 0;
}; // class CSpecialTableView
