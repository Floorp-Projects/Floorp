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

#ifndef __msg_Host__
#define __msg_Host__ 1

#include "msgzap.h"

class MSG_NewsHost;
class MSG_IMAPHost;

class MSG_Host : public MSG_ZapIt {

public:
    virtual XP_Bool IsIMAP () { return FALSE; }
    virtual XP_Bool IsNews () { return FALSE; }
	virtual MSG_NewsHost *GetNewsHost() { return 0; }
	virtual MSG_IMAPHost *GetIMAPHost() { return 0; }
	virtual const char* getFullUIName() { return 0; } 
	virtual int RemoveHost() {return 0;}
	virtual XP_Bool isSecure() {return FALSE;}
	virtual int32 getPort() { return 0; }
};



#endif /* __msg_Host__ */
