/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#ifndef __nsAccessibilityService_h__
#define __nsAccessibilityService_h__

#include "nsIAccessibilityService.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIDocShell.h"

class nsIFrame;
class nsIWeakReference;
class nsIDOMNode;

class nsAccessibilityService : public nsIAccessibilityService
{
public:
  NS_DECL_ISUPPORTS

  // nsIAccessibilityService methods:
  NS_DECL_NSIACCESSIBILITYSERVICE

  // nsAccessibilityService methods:
  nsAccessibilityService();
  virtual ~nsAccessibilityService();

public:

private:
  NS_IMETHOD GetInfo(nsISupports* aFrame, nsIFrame** aRealFrame, nsIWeakReference** aShell, nsIDOMNode** aContent);
  NS_IMETHOD GetShellFromNode(nsIDOMNode *aNode, nsIWeakReference **weakShell);
  void GetOwnerFor(nsIPresShell *aPresShell, nsIPresShell **aOwnerShell, nsIContent **aOwnerContent);
  nsIContent* FindContentForDocShell(nsIPresShell* aPresShell, nsIContent* aContent, nsIDocShell*  aDocShell);

};

#endif /* __nsIccessibilityService_h__ */
