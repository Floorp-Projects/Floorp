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

#include <LEditField.h>

//======================================
class CValidEditField: public LEditField
//======================================
{
public:
	enum { class_ID = 'vald' };
	typedef Boolean (*ValidationFunc)( CValidEditField* editField );
						CValidEditField( LStream* s );
	virtual Boolean		AllowTargetSwitch( LCommander* newTarget );
	void				SetValidationFunction( ValidationFunc validation );
protected:
	ValidationFunc		mValidationFunc;
}; // CValidEditField


//======================================
// Validation Functions
//======================================
Boolean ConstrainEditField( LEditField* whichField, long minValue, long maxValue );
void TargetOnEditField( LEditField* editField, Boolean doTarget );
Boolean ValidateDaysTilExpire( CValidEditField* daysTilExpire );
Boolean ValidateCacheSize( CValidEditField* bufferSize );
Boolean ValidateNumberNewsArticles( CValidEditField* articles );
Boolean ValidateNumberConnections( CValidEditField* connections );
Boolean ValidatePopID( CValidEditField* connections );
