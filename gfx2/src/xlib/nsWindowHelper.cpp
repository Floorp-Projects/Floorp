/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsWindowHelper.h"

#include "nsCOMPtr.h"

#include "nsIWindow.h"

static
PLHashNumber WindowHashKey(Window key)
{
  return (PLHashNumber) key;
}

PLHashTable *nsWindowHelper::sWindowMapper = PL_NewHashTable(20, (PLHashFunction)WindowHashKey,
                                                             PL_CompareValues, PL_CompareValues, 0, 0);

nsWindowHelper::nsWindowHelper()
{
}

nsWindowHelper::~nsWindowHelper()
{

}

/* static methods */
void nsWindowHelper::AddWindow(const Window aWindow, nsIWindow *aIWindow)
{

  PL_HashTableAdd(sWindowMapper, NS_REINTERPRET_CAST(const void *, aWindow), aIWindow);

}

void nsWindowHelper::RemoveWindow(const Window aWindow)
{
  PL_HashTableRemove(sWindowMapper, NS_REINTERPRET_CAST(const void *, aWindow));
}

nsIWindow *nsWindowHelper::FindIWindow(const Window aWindow)
{
  /* should this addref? */
  return NS_STATIC_CAST(nsIWindow*, PL_HashTableLookup(sWindowMapper, NS_REINTERPRET_CAST(const void *, aWindow)));
}
