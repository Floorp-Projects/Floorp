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

/*

FILE:			CSingleLineEditField.h
CLASS:			CSingleLineEditField
DESCRIPTION:	Filters out carriage returns when responding to a cmd_Paste
				This is the only change from LGAEditField
CREATION DATE:	97.06.03

---------------------------------------<< ¥ >>----------------------------------------

Overrides ObeyCommand to enable filtering of carriage returns from the clipboard.
Reason this fix is important is because there are fields where you really don't want
to permit carriage returns. In the preferences dialog and in the mail filters dialog
for example. Carriage returns end up rendering the files used by these dialogs
unreadable by the parser

Inherits from LGAEditField so it has a gray scale appearance

---------------------------------------<< ¥ >>----------------------------------------
*/

#pragma once

#include "SearchHelpers.h"

class CSingleLineEditField : public CSearchEditField {

	public:
	
		enum	{ class_ID = 'sLEF' };
		
							CSingleLineEditField (LStream* inStream);

		virtual				~CSingleLineEditField ();
		
};
