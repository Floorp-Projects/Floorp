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

#include "LTextFile.h"

class LWinIniFile: public LTextFile
{
public:
					LWinIniFile( FSSpec& inFileSpec );
					LWinIniFile();
					
	virtual ExceptionCode	FindSection( const LStr255& inSectionName );
	virtual ExceptionCode	GetValueForName( const LStr255& inName, LStr255& outValue );

	virtual Int16			OpenDataFork( Int16 inPermissions );
	virtual void			CloseDataFork();
protected:
	virtual	ExceptionCode	GetNextNameValuePair( LStr255& pName, LStr255& pValue );

	Int32			mSectionPosition;
};

