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
 * Contributor(s): 
 */


#include "nsIURIRefObject.h"

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsString.h"

#ifndef nsHTMLURIRefObject_h__
#define nsHTMLURIRefObject_h__

#define NS_URI_REF_OBJECT_CID                          \
{ /* {bdd79df6-1dd1-11b2-b29c-c3d63a58f1d2} */         \
    0xbdd79df6, 0x1dd1, 0x11b2,                        \
    { 0xb2, 0x9c, 0xc3, 0xd6, 0x3a, 0x58, 0xf1, 0xd2 } \
}

class nsHTMLURIRefObject : public nsIURIRefObject
{
public:
  nsHTMLURIRefObject();
  virtual ~nsHTMLURIRefObject();

  // Interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

  NS_DECL_NSIURIREFOBJECT

protected:
  nsCOMPtr<nsIDOMNode> mNode;
  nsCOMPtr<nsIDOMNamedNodeMap> mAttributes;
  PRUint32 mCurAttrIndex;
  PRUint32 mAttributeCnt;
};

nsresult NS_NewHTMLURIRefObject(nsIURIRefObject** aResult, nsIDOMNode* aNode);

#endif /* nsHTMLURIRefObject_h__ */

