/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

#ifndef __inDeepTreeWalker_h___
#define __inDeepTreeWalker_h___

#include "inIDeepTreeWalker.h"

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsVoidArray.h"

class inDeepTreeWalker : public inIDeepTreeWalker
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIDOMTREEWALKER
	NS_DECL_INIDEEPTREEWALKER

  inDeepTreeWalker();
  virtual ~inDeepTreeWalker();

protected:
  void PushNode(nsIDOMNode* aNode);

  PRBool mShowAnonymousContent;
  PRBool mShowSubDocuments;
  nsCOMPtr<nsIDOMNode> mRoot;
  nsCOMPtr<nsIDOMNode> mCurrentNode;
  PRUint32 mWhatToShow;
  
  nsAutoVoidArray mStack;
};

#endif // __inDeepTreeWalker_h___
