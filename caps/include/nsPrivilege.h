/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
/*describes the state and duration of privileges*/
#ifndef _NS_PRIVILEGE_H_
#define _NS_PRIVILEGE_H_

#include "nsIPrivilege.h"
#include "prtypes.h"

class nsPrivilege : public nsIPrivilege {

public:

	NS_DECL_ISUPPORTS

	NS_IMETHOD
	GetState(PRInt16 * state);

	NS_IMETHOD 
	SetState(PRInt16 state);

	NS_IMETHOD 
	GetDuration(PRInt16 * duration);

	NS_IMETHOD 
	SetDuration(PRInt16 duration);

	NS_IMETHOD
	SameState(nsIPrivilege * other, PRBool * result);

	NS_IMETHOD
	SameDuration(nsIPrivilege * other, PRBool * result);

	NS_IMETHOD
	IsAllowed(PRBool * result);

	NS_IMETHOD
	IsForbidden(PRBool * result);

	NS_IMETHOD
	IsBlank(PRBool * result);

	NS_IMETHOD
	ToString(char * * result);

	NS_IMETHOD
	Equals(nsIPrivilege * perm, PRBool * res);

	nsPrivilege(PRInt16 state, PRInt16 duration);
	virtual ~nsPrivilege(void);

protected:
	PRInt16 itsState;
	PRInt16 itsDuration;
	char * itsString;
};

#endif /* _NS_PRIVILEGE_H_ */
