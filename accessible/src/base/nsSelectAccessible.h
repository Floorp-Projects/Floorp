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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */
#ifndef __nsSelectAccessible_h__
#define __nsSelectAccessible_h__

#include "nsAccessible.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"

class nsSelectAccessible : public nsAccessible
{
public:
  
  nsSelectAccessible(nsIAtom* aPopupAtom, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccName(PRUnichar **_retval);
  NS_IMETHOD GetAccValue(PRUnichar **_retval);
  NS_IMETHOD GetAccRole(PRUnichar **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);

  virtual ~nsSelectAccessible() {}
  virtual nsIAccessible* CreateNewFirstAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  virtual nsIAccessible* CreateNewLastAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);

  nsCOMPtr<nsIAtom> mPopupAtom;
};


#endif
