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

#include "LGAEditField.h"
const MessageT msg_EditField2 = 'Ctrn';

/* CBigGrayEditField is the LGAEditField with additional calls to handle c-strings
   (more than 255 characters).  */

class CLargeEditField : public LGAEditField
{
	public:
		enum { class_ID = 'Ledt' };

									CLargeEditField( LStream* inStream ) : LGAEditField ( inStream ) {};

	// c-string members
		char *	GetLongDescriptor();	// ALLOCATES MEMORY!
		void	SetLongDescriptor( char *inDescriptor );
};


// this class is used to broadcast UserChangedText
class CLargeEditFieldBroadcast : public CLargeEditField
{
	public:
		enum { class_ID = 'LedB' };

						CLargeEditFieldBroadcast( LStream* inStream ) : CLargeEditField ( inStream ) {};
	
		virtual void	UserChangedText() { BroadcastMessage(msg_EditField2, this); };
};

