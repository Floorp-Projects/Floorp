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

#ifndef nsSOAPEncodingRegistry_h__
#define nsSOAPEncodingRegistry_h__

#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsISOAPEncoding.h"
#include "nsISOAPEncoder.h"
#include "nsISOAPDecoder.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsISchema.h"

class nsSOAPEncoding;

/* Header file */
class nsSOAPEncodingRegistry:public nsISOAPEncoding {
public:
  NS_DECL_ISUPPORTS NS_DECL_NSISOAPENCODING nsSOAPEncodingRegistry() {
  } nsSOAPEncodingRegistry(nsISOAPEncoding * aEncoding);
  virtual ~ nsSOAPEncodingRegistry();
protected:
  nsSupportsHashtable * mEncodings;
  nsCOMPtr < nsISchemaCollection > mSchemaCollection;
};

class nsSOAPEncoding:public nsISOAPEncoding {
public:
  NS_DECL_ISUPPORTS NS_DECL_NSISOAPENCODING nsSOAPEncoding();
  nsSOAPEncoding(const nsAString & aStyleURI,
		 nsSOAPEncodingRegistry * aRegistry,
		 nsISOAPEncoding * aDefaultEncoding);
  virtual ~ nsSOAPEncoding();
  /* additional members */

protected:
  nsString mStyleURI;
  nsSupportsHashtable *mEncoders;
  nsSupportsHashtable *mDecoders;
  nsCOMPtr < nsISOAPEncoding > mRegistry;
  nsCOMPtr < nsISOAPEncoding > mDefaultEncoding;
  nsCOMPtr < nsISOAPEncoder > mDefaultEncoder;
  nsCOMPtr < nsISOAPDecoder > mDefaultDecoder;
  nsSupportsHashtable *mMappedInternal;
  nsSupportsHashtable *mMappedExternal;
};
#endif
