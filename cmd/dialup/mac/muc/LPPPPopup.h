/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#pragma once
#include "MUC.h"
#include <LGAPopup.h>

class LPPPPopup: public LGAPopup
{
public:
	enum { class_ID = 'PPPP' };
	
	static void*	CreatePPPPopup( LStream* inStream );
					LPPPPopup( LStream* inStream );
					
	void			SetPPPFunction( TraversePPPListFunc p );
	void			UpdateList();

	Boolean			SetToNamedItem( const LStr255& name );
	void			GetNameValue( LStr255& value );
protected:	
	TraversePPPListFunc		mFunction;
};

