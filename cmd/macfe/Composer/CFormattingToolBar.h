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

#include "CAMSavvyBevelView.h"
#include <LListener.h>

class CEditView;


// used in Editor window and Mail Compose windows

class CFormattingToolBar : public CAMSavvyBevelView, public LListener
{
public:
	enum {class_ID = 'FoTB'};
					CFormattingToolBar(LStream * inStream);
					~CFormattingToolBar();

	virtual void	ListenToMessage( MessageT inMessage, void* ioParam );
	virtual void	FinishCreateSelf();

protected:
	CEditView* 		mEditView;
};

