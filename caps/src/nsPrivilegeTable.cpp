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

#include "nsPrivilegeTable.h"
#include "nsPrivilegeManager.h"

nsPrivilegeTable::nsPrivilegeTable(void)
{
	this->itsTable = NULL;
}

nsPrivilegeTable::~nsPrivilegeTable(void)
{
	/* XXX: We need to incr and decr objects that we put into this hashtable.
	 * There is a big memory leak.
	 * We need to delete all the entries in the privilege Table
	 */
	if (this->itsTable) delete this->itsTable;
	this->itsTable = NULL;
}

PRInt32
nsPrivilegeTable::Size(void)
{
	return (this->itsTable != NULL) ? this->itsTable->Count() : 0;
}

PRBool
nsPrivilegeTable::IsEmpty(void)
{
	return ((this->itsTable == NULL) && (this->itsTable->Count() == 0)) ? PR_TRUE : PR_FALSE;
}

nsIPrivilege * 
nsPrivilegeTable::Get(nsTarget *target)
{
	if (itsTable == NULL) 
		return nsPrivilegeManager::FindPrivilege(nsIPrivilege::PrivilegeState_Blank, nsIPrivilege::PrivilegeDuration_Session);
	TargetKey targKey(target);
	nsIPrivilege * priv = (nsIPrivilege *) this->itsTable->Get(& targKey);
	return (priv == NULL)
	? nsPrivilegeManager::FindPrivilege(nsIPrivilege::PrivilegeState_Blank, nsIPrivilege::PrivilegeDuration_Session)
	: priv;
}

nsIPrivilege * 
nsPrivilegeTable::Put(nsTarget * target, nsIPrivilege * priv)
{
	nsCaps_lock();
	if (itsTable == NULL) this->itsTable = new nsHashtable();
	TargetKey targKey(target);
	nsIPrivilege * priv2 = (nsIPrivilege *)this->itsTable->Put(& targKey, (void *)priv);
	nsCaps_unlock();
	return priv2;
}

nsIPrivilege * 
nsPrivilegeTable::Remove(nsTarget * target)
{
	if (itsTable == NULL) return NULL;
	TargetKey targKey(target);
	nsCaps_lock();
	nsIPrivilege * priv = (nsIPrivilege *)this->itsTable->Remove(& targKey);
	nsCaps_unlock();
	return priv;
}

nsPrivilegeTable * 
nsPrivilegeTable::Clone(void)
{
	nsCaps_lock();
	nsPrivilegeTable *newbie = new nsPrivilegeTable();
	if (itsTable != NULL) newbie->itsTable = itsTable->Clone();
	nsCaps_unlock();
	return newbie;
}

void 
nsPrivilegeTable::Clear(void)
{
	/* XXX: free the entries also */
	nsCaps_lock();
	if (this->itsTable) delete this->itsTable;
	nsCaps_unlock();
}

void 
nsPrivilegeTable::Enumerate(nsHashtableEnumFunc aEnumFunc) {
	if (itsTable != NULL) itsTable->Enumerate(aEnumFunc);
}
