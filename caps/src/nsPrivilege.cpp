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

#include "nsPrivilege.h"
#include "xp.h"

static NS_DEFINE_IID(kIPrivilegeIID, NS_IPRIVILEGE_IID);

NS_IMPL_ISUPPORTS(nsPrivilege, kIPrivilegeIID);

NS_IMETHODIMP
nsPrivilege::GetState(PRInt16 * state)
{
	* state = itsState;
	return (itsState) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsPrivilege::SetState(PRInt16 state)
{
	itsState=state;	
	return NS_OK;
}

NS_IMETHODIMP 
nsPrivilege::GetDuration(PRInt16 * duration)
{
	* duration = itsDuration;
	return (itsDuration) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsPrivilege::SetDuration(PRInt16 duration)
{
	itsDuration=duration;
	return NS_OK;
}

NS_IMETHODIMP
nsPrivilege::SameState(nsIPrivilege * other, PRBool * result)
{
	nsresult rv;
	PRInt16 * myState = 0, * otherState = 0;
	rv = this->GetState(myState);
	rv = other->GetState(otherState);
	* result = (otherState == myState) ? PR_TRUE : PR_FALSE;
	return rv;
}

NS_IMETHODIMP
nsPrivilege::SameDuration(nsIPrivilege * other, PRBool * result)
{
	nsresult rv;
	PRInt16 * myDur =0, * otherDur = 0;
	rv = this->GetState(myDur);
	rv = other->GetState(otherDur);
	* result = (otherDur == myDur) ? PR_TRUE : PR_FALSE;
	return rv;
}

NS_IMETHODIMP
nsPrivilege::IsAllowed(PRBool * result) 
{	
	nsresult rv;
	PRInt16 * myState = 0;
	rv = this->GetState(myState);
	* result = (myState == (PRInt16 *)nsIPrivilege::PrivilegeState_Allowed) ? PR_TRUE : PR_FALSE;
	return rv;
}

NS_IMETHODIMP
nsPrivilege::IsForbidden(PRBool * result)
{
	nsresult rv;
	PRInt16 * myState = 0;
	rv = this->GetState(myState);
	* result = (myState == (PRInt16 *)nsIPrivilege::PrivilegeState_Forbidden) ? PR_TRUE : PR_FALSE;
	return rv;
}

NS_IMETHODIMP
nsPrivilege::IsBlank(PRBool * result)
{
	nsresult rv;
	PRInt16 * myState = 0;
	rv = this->GetState(myState);
	* result = (myState == (PRInt16 *)nsIPrivilege::PrivilegeState_Blank) ? PR_TRUE : PR_FALSE;
	return rv;
}

NS_IMETHODIMP
nsPrivilege::ToString(char * * result)
{
	char * privStr = NULL;
	char * durStr = NULL;
	if (itsString != NULL) {
		result=&itsString;
		return NS_OK;
	}
	PRInt16 temp;
	this->GetState(& temp);
	switch(temp) {
		case nsIPrivilege::PrivilegeState_Allowed:
			privStr = "allowed";
			break;
		case nsIPrivilege::PrivilegeState_Forbidden:
			privStr = "forbidden";
			break;
		case nsIPrivilege::PrivilegeState_Blank:
			privStr = "blank";
			break;
		default:
			PR_ASSERT(FALSE);
			privStr = "error";
			break;
	}
	this->GetDuration(& temp);
	switch(temp) {
		case nsIPrivilege::PrivilegeDuration_Scope:
			durStr = " in scope";
			break;
		case nsIPrivilege::PrivilegeDuration_Session:
			durStr = " in session";
			break;
		case nsIPrivilege::PrivilegeDuration_Forever:
			durStr = " forever";
			break;
		case nsIPrivilege::PrivilegeDuration_Blank:
			privStr = "blank";
			break;
		default:
			PR_ASSERT(FALSE);
			privStr = "error";
			break;
	}
	itsString = new char[strlen(privStr) + strlen(durStr) + 1];
	XP_STRCPY(itsString, privStr);
	XP_STRCAT(itsString, durStr);
	result = & itsString;
	return NS_OK;
}

NS_IMETHODIMP
nsPrivilege::Equals(nsIPrivilege * other, PRBool * result)
{
	nsresult rv;
	PRBool sameState = PR_FALSE, sameDuration = PR_FALSE;
	rv = this->SameState(other, & sameState);
	rv = this->SameDuration(other, & sameDuration);
	* result = (sameState && sameDuration) ? PR_TRUE : PR_FALSE;
	return rv;
}

nsPrivilege::nsPrivilege(PRInt16 state, PRInt16 duration)
{
	itsState = state;
	itsDuration = duration;
	itsString = NULL;
}

nsPrivilege::~nsPrivilege(void)
{
	if(itsString) delete [] itsString;
}
