/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsIDocumentEncoder.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_CID(kCHTMLEncoderCID, NS_HTML_ENCODER_CID);

class nsHTMLEncoder : public nsIHTMLEncoder
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOCUMENT_ENCODER_IID; return iid; }

  nsHTMLEncoder();
  virtual ~nsHTMLEncoder();

  /* Interfaces for addref and release and queryinterface */
  NS_DECL_ISUPPORTS

  // Inherited methods from nsIDocument 
  NS_IMETHOD SetSelection(nsIDOMSelection* aSelection);
  NS_IMETHOD SetCharset(const nsString& aCharset);
  NS_IMETHOD Encode(nsIOutputStream* aStream);

  // Get embedded objects -- images, links, etc.
  // NOTE: we may want to use an enumerator
  NS_IMETHOD GetEmbeddedObjects(nsISupportsArray* aObjects);
  NS_IMETHOD SubstituteURL(const nsString& aOriginal,
                           const nsString& aReplacement);
  NS_IMETHOD PrettyPrint(PRBool aYesNO);
};

NS_IMPL_ADDREF(nsHTMLEncoder)
NS_IMPL_RELEASE(nsHTMLEncoder)

nsHTMLEncoder::nsHTMLEncoder()
{
}

nsHTMLEncoder::~nsHTMLEncoder()
{
}

nsresult nsHTMLEncoder::QueryInterface(REFNSIID aIID,   
                                               void **aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;   

  *aInstancePtr = 0;   

  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(nsIDocumentEncoder::GetIID())) {
    *aInstancePtr = (void *)(nsIDocumentEncoder*)this;   
  }

  if (nsnull == *aInstancePtr)
    return NS_NOINTERFACE;   

  NS_ADDREF_THIS();

  return NS_OK;   
}

NS_IMETHODIMP
nsHTMLEncoder::SetSelection(nsIDOMSelection* aSelection)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEncoder::SetCharset(const nsString& aCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEncoder::Encode(nsIOutputStream* aStream)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEncoder::GetEmbeddedObjects(nsISupportsArray* aObjects)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLEncoder::SubstituteURL(const nsString& aOriginal, const nsString& aReplacement)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
              
NS_IMETHODIMP
nsHTMLEncoder::PrettyPrint(PRBool aYes)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NS_NewHTMLEncoder(nsIDocumentEncoder** aResult)
{
  *aResult = new nsHTMLEncoder;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

class nsDocumentEncoderFactory : public nsIFactory
{
public:   
  nsDocumentEncoderFactory();   
  virtual ~nsDocumentEncoderFactory();   

  // nsISupports methods   
  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            REFNSIID aIID,
                            void **aResult);
};   

nsDocumentEncoderFactory::nsDocumentEncoderFactory()
{
  mRefCnt = 0;
}

nsDocumentEncoderFactory::~nsDocumentEncoderFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}

NS_IMPL_ADDREF(nsDocumentEncoderFactory)
NS_IMPL_RELEASE(nsDocumentEncoderFactory)

nsresult nsDocumentEncoderFactory::QueryInterface(REFNSIID aIID,   
                                               void **aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;   

  *aInstancePtr = 0;   

  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(kIFactoryIID)) {
    *aInstancePtr = (void *)(nsIFactory*)this;   
  }

  if (nsnull == *aInstancePtr)
    return NS_NOINTERFACE;   

  NS_ADDREF_THIS();

  return NS_OK;   
}

nsresult
nsDocumentEncoderFactory::CreateInstance(nsISupports *aOuter,
                                         REFNSIID aIID,
                                         void **aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;  

  *aResult = 0;

  if (aIID.Equals(kCHTMLEncoderCID))
    *aResult = new nsHTMLEncoder;
  else
    return NS_NOINTERFACE;

  if (*aResult == NULL)
    return NS_ERROR_OUT_OF_MEMORY;  

  return NS_OK;
}

