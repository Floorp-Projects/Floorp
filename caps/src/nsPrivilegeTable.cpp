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

//
// 			PUBLIC METHODS 
//

nsPrivilegeTable::nsPrivilegeTable(void)
{
  itsTable = NULL;
}

nsPrivilegeTable::~nsPrivilegeTable(void)
{
  /* XXX: We need to incr and decr objects that we put into this hashtable.
   * There is a big memory leak.
   *
   * We need to delete all the entries in the privilege Table
   */
  if (itsTable)
    delete itsTable;
  itsTable = NULL;
}

PRInt32 nsPrivilegeTable::size(void)
{
  if (itsTable != NULL) 
    return itsTable->Count();
  else return 0;
}

PRBool nsPrivilegeTable::isEmpty(void)
{
  if (itsTable == NULL) return PR_TRUE;
  else return ((itsTable->Count() > 0) ? PR_FALSE : PR_TRUE);
}

nsPrivilege * nsPrivilegeTable::get(nsTarget *target)
{
  if (itsTable == NULL) {
    return nsPrivilege::findPrivilege(nsPermissionState_Blank,
                                      nsDurationState_Session);
  }

  TargetKey targKey(target);
  
  nsPrivilege *priv = (nsPrivilege *) itsTable->Get(&targKey);
  if (priv == NULL) {
    return nsPrivilege::findPrivilege(nsPermissionState_Blank,
                                      nsDurationState_Session);
  }
  return priv;
}

nsPrivilege * nsPrivilegeTable::put(nsTarget *target, nsPrivilege *priv)
{
  nsCaps_lock();
  if (itsTable == NULL) itsTable = new nsHashtable();

  TargetKey targKey(target);
  nsPrivilege *priv2 = (nsPrivilege *)itsTable->Put(&targKey, (void *)priv);
  nsCaps_unlock();
  return priv2;
}

nsPrivilege * nsPrivilegeTable::remove(nsTarget *target)
{
  if (itsTable == NULL) return NULL;
  TargetKey targKey(target);
  nsCaps_lock();
  nsPrivilege *priv = (nsPrivilege *)itsTable->Remove(&targKey);
  nsCaps_unlock();
  return priv;
}

nsPrivilegeTable * nsPrivilegeTable::clone(void)
{
  nsCaps_lock();
  nsPrivilegeTable *newbie = new nsPrivilegeTable();
  if (itsTable != NULL) {
    newbie->itsTable = itsTable->Clone();
  }
  nsCaps_unlock();
  return newbie;
}

void nsPrivilegeTable::clear(void)
{
  /* XXX: free the entries also */
  nsCaps_lock();
  delete itsTable;
  itsTable = NULL;
  nsCaps_unlock();
}

void nsPrivilegeTable::Enumerate(nsHashtableEnumFunc aEnumFunc) {
  if (itsTable != NULL) {
    itsTable->Enumerate(aEnumFunc);
  }
}
