/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is
 * David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein <Deinst@world.std.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozCStr2CStrHashtable.h"
#include "nsCRT.h"
#include "nsMemory.h"

static void* PR_CALLBACK
CloneCString(nsHashKey *aKey, void *aData, void* closure)
{
  return nsCRT::strdup((const char*)aData);
}

static PRBool PR_CALLBACK
DeleteCString(nsHashKey *aKey, void *aData, void* closure)
{
  nsMemory::Free((char*)aData);
  return PR_TRUE;
}

mozCStr2CStrHashtable::mozCStr2CStrHashtable()
    : mHashtable(CloneCString, nsnull, DeleteCString, nsnull, 16)
{
}

mozCStr2CStrHashtable::~mozCStr2CStrHashtable()
{
}

nsresult
mozCStr2CStrHashtable::Put(const char *key, const char* aData)
{
  char* value = strdup(aData);
  if (value == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  nsCStringKey k(key);
  char* oldValue = (char*)mHashtable.Put(&k, value);
  if (oldValue)
    free(oldValue);
  return NS_OK;
}

char* 
mozCStr2CStrHashtable::Get(const char *key)
{
  nsCStringKey k(key);
  const char* value = (const char*)mHashtable.Get(&k);
  if (value == nsnull)
    return nsnull;
  return strdup(value);
}

nsresult
mozCStr2CStrHashtable::Remove(const char *key)
{
  nsCStringKey k(key);
  char* oldValue = (char*)mHashtable.Remove(&k);
  if (oldValue)
    nsMemory::Free(oldValue);
  return NS_OK;
}

void
mozCStr2CStrHashtable::Reset()
{
    mHashtable.Reset();
}
