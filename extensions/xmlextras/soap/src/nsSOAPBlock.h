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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsSOAPBlock_h__
#define nsSOAPBlock_h__

#include "nsString.h"
#include "nsIVariant.h"
#include "nsISOAPBlock.h"
#include "nsIJSNativeInitializer.h"
#include "nsISOAPEncoding.h"
#include "nsISchema.h"
#include "nsIDOMElement.h"
#include "nsISOAPAttachments.h"
#include "nsCOMPtr.h"

class nsSOAPBlock:public nsISOAPBlock, public nsIJSNativeInitializer {
public:
  nsSOAPBlock();
  virtual ~ nsSOAPBlock();

  NS_DECL_ISUPPORTS
      // nsISOAPBlock
  NS_DECL_NSISOAPBLOCK
      // nsIJSNativeInitializer
  NS_IMETHOD Initialize(JSContext * cx, JSObject * obj,
                        PRUint32 argc, jsval * argv);

protected:
  nsString mNamespaceURI;
  nsString mName;
   nsCOMPtr < nsISOAPEncoding > mEncoding;
   nsCOMPtr < nsISchemaType > mSchemaType;
   nsCOMPtr < nsISOAPAttachments > mAttachments;
   nsCOMPtr < nsIDOMElement > mElement;
   nsCOMPtr < nsIVariant > mValue;
  nsresult mStatus;
  PRBool mComputeValue;
  PRBool mVersion;
};

#endif
