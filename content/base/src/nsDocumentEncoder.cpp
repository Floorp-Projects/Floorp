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

#include "nsIDocumentEncoder.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIDOMSelection.h"
#include "nsIPresShell.h"
#include "nsXIFDTD.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsHTMLContentSinkStream.h"
#include "nsHTMLToTXTSinkStream.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_CID(kCHTMLEncoderCID, NS_HTML_ENCODER_CID);
static NS_DEFINE_CID(kCTextEncoderCID, NS_TEXT_ENCODER_CID);

class nsHTMLEncoder : public nsIHTMLEncoder
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOCUMENT_ENCODER_IID; return iid; }

  nsHTMLEncoder();
  virtual ~nsHTMLEncoder();

  NS_IMETHOD Init(nsIPresShell* aPresShell, nsIDocument* aDocument, nsString& aMimeType);

  /* Interfaces for addref and release and queryinterface */
  NS_DECL_ISUPPORTS

  // Inherited methods from nsIDocument 
  NS_IMETHOD SetSelection(nsIDOMSelection* aSelection);
  NS_IMETHOD SetCharset(const nsString& aCharset);
  NS_IMETHOD EncodeToStream(nsIOutputStream* aStream);
  NS_IMETHOD EncodeToString(nsString& aOutputString);

  // Get embedded objects -- images, links, etc.
  // NOTE: we may want to use an enumerator
  NS_IMETHOD GetEmbeddedObjects(nsISupportsArray* aObjects);
  NS_IMETHOD SubstituteURL(const nsString& aOriginal,
                           const nsString& aReplacement);
  NS_IMETHOD PrettyPrint(PRBool aYesNO);

private:
  nsIDocument*      mDocument;
  nsIDOMSelection*  mSelection;
  nsIPresShell*     mPresShell;
  nsString          mMimeType;
  nsString          mCharset;
};



NS_IMPL_ADDREF(nsHTMLEncoder)
// NS_IMPL_RELEASE(nsHTMLEncoder)
NS_IMETHODIMP_(nsrefcnt) nsHTMLEncoder::Release(void)                         
{                                                      
  NS_PRECONDITION(0 != mRefCnt, "dup release");        
  if (--mRefCnt == 0) {                                
    NS_DELETEXPCOM(this);                              
    return 0;                                          
  }                                                    
  return mRefCnt;                                      
}
nsHTMLEncoder::nsHTMLEncoder() : mMimeType("text/html")
{
  mDocument = 0;
  mSelection = 0;
  mPresShell = 0;
  NS_INIT_REFCNT();
}

nsHTMLEncoder::~nsHTMLEncoder()
{
  NS_IF_RELEASE(mDocument); 
  NS_IF_RELEASE(mSelection);
  NS_IF_RELEASE(mPresShell);
}

NS_IMETHODIMP
nsHTMLEncoder::Init(nsIPresShell* aPresShell, nsIDocument* aDocument, nsString& aMimeType)
{
  if (!aDocument)
    return NS_ERROR_INVALID_ARG;

  mDocument = aDocument;
  NS_ADDREF(mDocument);
  mPresShell = aPresShell;
  NS_ADDREF(aPresShell);
  mMimeType = aMimeType;
  return NS_OK;
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
  mSelection = aSelection;
  return NS_OK;   
}

NS_IMETHODIMP
nsHTMLEncoder::SetCharset(const nsString& aCharset)
{
  mCharset = aCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEncoder::EncodeToString(nsString& aOutputString)
{
  nsresult rv;

  if (!mDocument)
    return NS_ERROR_NOT_INITIALIZED;
  if (!mPresShell)
    return NS_ERROR_NOT_INITIALIZED;

  // xxx Also make sure mString is a mime type "text/html" or "text/plain"
  
  if (mPresShell) {
    if (mDocument) {
      nsString buffer;

      mDocument->CreateXIF(buffer,mSelection);
      nsIParser* parser;

      static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
      static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

      rv = nsComponentManager::CreateInstance(kCParserCID, 
                                                 nsnull, 
                                                 kCParserIID, 
                                                 (void **)&parser);

      if (NS_OK == rv) {
        nsIHTMLContentSink* sink = nsnull;

        rv = NS_New_HTML_ContentSinkStream(&sink,&aOutputString);
  
      	if (sink && NS_SUCCEEDED(rv)) {

	        if (NS_OK == rv) {
	          parser->SetContentSink(sink);
	   
	          nsIDTD* dtd = nsnull;
	          rv = NS_NewXIFDTD(&dtd);
	          if (NS_OK == rv) {
	            parser->RegisterDTD(dtd);
	            parser->Parse(buffer, 0, "text/xif",PR_FALSE,PR_TRUE);           
	          }
	          NS_IF_RELEASE(dtd);
	          NS_IF_RELEASE(sink);
	        }
        }
        NS_RELEASE(parser);
      }
  	}
	}
  return rv;
}

NS_IMETHODIMP
nsHTMLEncoder::EncodeToStream(nsIOutputStream* aStream)
{
  nsresult rv;

  if (!mDocument)
    return NS_ERROR_NOT_INITIALIZED;
  if (!mPresShell)
    return NS_ERROR_NOT_INITIALIZED;

  // xxx Also make sure mString is a mime type "text/html" or "text/plain"
  
  if (mPresShell) {
    if (mDocument) {
      nsString buffer;

      mDocument->CreateXIF(buffer,mSelection);
      
      nsString*     charset = nsnull;
      nsAutoString  defaultCharset("ISO-8859-1");
      if (!mCharset.Equals("null") && !mCharset.Equals(""))
        charset = &mCharset; 

      nsIParser* parser;

      static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
      static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

      rv = nsComponentManager::CreateInstance(kCParserCID, 
                                                 nsnull, 
                                                 kCParserIID, 
                                                 (void **)&parser);

      if (NS_OK == rv) {
        nsIHTMLContentSink* sink = nsnull;

        rv = NS_New_HTML_ContentSinkStream(&sink,aStream,charset);
  
      	if (sink && NS_SUCCEEDED(rv)) {

	        if (NS_OK == rv) {
	          parser->SetContentSink(sink);
	    
	          nsIDTD* dtd = nsnull;
	          rv = NS_NewXIFDTD(&dtd);
	          if (NS_OK == rv) {
	            parser->RegisterDTD(dtd);
	            parser->Parse(buffer, 0, "text/xif",PR_FALSE,PR_TRUE);           
	          }
	          NS_IF_RELEASE(dtd);
	          NS_IF_RELEASE(sink);
	        }
        }
        NS_RELEASE(parser);
      }
  	}
	}
  return rv;


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
  NS_ADDREF(*aResult);
  return NS_OK;
}


class nsTextEncoder : public nsITextEncoder
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOCUMENT_ENCODER_IID; return iid; }

  nsTextEncoder();
  virtual ~nsTextEncoder();

  NS_IMETHOD Init(nsIPresShell* aPresShell, nsIDocument* aDocument, nsString& aMimeType);

  /* Interfaces for addref and release and queryinterface */
  NS_DECL_ISUPPORTS

  // Inherited methods from nsIDocument 
  NS_IMETHOD SetSelection(nsIDOMSelection* aSelection);
  NS_IMETHOD SetCharset(const nsString& aCharset);
  NS_IMETHOD EncodeToStream(nsIOutputStream* aStream);
  NS_IMETHOD EncodeToString(nsString& aOutputString);

  NS_IMETHOD PrettyPrint(PRBool aYesNO);

private:
  nsIDocument*      mDocument;
  nsIDOMSelection*  mSelection;
  nsIPresShell*     mPresShell;
  nsString          mMimeType;
  nsString          mCharset;
};


NS_IMPL_ADDREF(nsTextEncoder)
NS_IMPL_RELEASE(nsTextEncoder)

nsTextEncoder::nsTextEncoder() : mMimeType("text/plain")
{
  NS_INIT_REFCNT();
  mDocument = 0;
  mSelection = 0;
}

nsTextEncoder::~nsTextEncoder()
{
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mSelection);
  NS_IF_RELEASE(mPresShell);
}

NS_IMETHODIMP
nsTextEncoder::Init(nsIPresShell* aPresShell, nsIDocument* aDocument, nsString& aMimeType)
{
  if (!aDocument)
    return NS_ERROR_INVALID_ARG;

  if (!aPresShell)
    return NS_ERROR_INVALID_ARG;

  mDocument = aDocument;
  NS_ADDREF(mDocument);
  mPresShell = aPresShell;
  NS_ADDREF(aPresShell);
  mMimeType = aMimeType;
  return NS_OK;
}

nsresult nsTextEncoder::QueryInterface(REFNSIID aIID,   
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
nsTextEncoder::SetSelection(nsIDOMSelection* aSelection)
{
  mSelection = aSelection;
  return NS_OK;   
}

NS_IMETHODIMP
nsTextEncoder::SetCharset(const nsString& aCharset)
{
  mCharset = aCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsTextEncoder::EncodeToString(nsString& aOutputString)
{
 nsresult rv;

  if (!mDocument)
    return NS_ERROR_NOT_INITIALIZED;
  if (!mPresShell)
    return NS_ERROR_NOT_INITIALIZED;

  // xxx Also make sure mString is a mime type "text/html" or "text/plain"
  
  if (mPresShell) {
    if (mDocument) {
      nsString buffer;

      mDocument->CreateXIF(buffer,mSelection);
      nsIParser* parser;

      static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
      static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

      rv = nsComponentManager::CreateInstance(kCParserCID, 
                                                 nsnull, 
                                                 kCParserIID, 
                                                 (void **)&parser);

      if (NS_OK == rv) {
        nsIHTMLContentSink* sink = nsnull;

        rv = NS_New_HTMLToTXT_SinkStream(&sink,&aOutputString);
  
      	if (sink && NS_SUCCEEDED(rv)) {

	        if (NS_OK == rv) {
	          parser->SetContentSink(sink);
	   
	          nsIDTD* dtd = nsnull;
	          rv = NS_NewXIFDTD(&dtd);
	          if (NS_OK == rv) {
	            parser->RegisterDTD(dtd);
	            parser->Parse(buffer, 0, "text/xif",PR_FALSE,PR_TRUE);           
	          }
	          NS_IF_RELEASE(dtd);
	          NS_IF_RELEASE(sink);
	        }
        }
        NS_RELEASE(parser);
      }
  	}
	}
  return rv;
}

NS_IMETHODIMP
nsTextEncoder::EncodeToStream(nsIOutputStream* aStream)
{
  nsresult rv;

  if (!mDocument)
    return NS_ERROR_NOT_INITIALIZED;
  if (!mPresShell)
    return NS_ERROR_NOT_INITIALIZED;

  // xxx Also make sure mString is a mime type "text/html" or "text/plain"
  
  if (mPresShell) {
    if (mDocument) {
      nsString buffer;

      mDocument->CreateXIF(buffer,mSelection);
      
      nsString*     charset = nsnull;
      nsAutoString  defaultCharset("ISO-8859-1");
      if (!mCharset.Equals("null") && !mCharset.Equals(""))
        charset = &mCharset; 

      nsIParser* parser;

      static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
      static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

      rv = nsComponentManager::CreateInstance(kCParserCID, 
                                                 nsnull, 
                                                 kCParserIID, 
                                                 (void **)&parser);

      if (NS_OK == rv) {
        nsIHTMLContentSink* sink = nsnull;

        rv = NS_New_HTMLToTXT_SinkStream(&sink,aStream,charset);
  
      	if (sink && NS_SUCCEEDED(rv)) {

	        if (NS_OK == rv) {
	          parser->SetContentSink(sink);

            nsIDTD* dtd = nsnull;
	          rv = NS_NewXIFDTD(&dtd);
	          if (NS_OK == rv) {
	            parser->RegisterDTD(dtd);
	            parser->Parse(buffer, 0, "text/xif",PR_FALSE,PR_TRUE);           
	          }
	          NS_IF_RELEASE(dtd);
	          NS_IF_RELEASE(sink);
	        }
        }
        NS_RELEASE(parser);
      }
  	}
	}
  return rv;
}

              
NS_IMETHODIMP
nsTextEncoder::PrettyPrint(PRBool aYes)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NS_NewTextEncoder(nsIDocumentEncoder** aResult)
{
  *aResult = new nsTextEncoder;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
 NS_ADDREF(*aResult);
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
  else if (aIID.Equals(kCTextEncoderCID))
    *aResult = new nsTextEncoder;
  else
    return NS_NOINTERFACE;

  if (*aResult == NULL)
    return NS_ERROR_OUT_OF_MEMORY;  

  return NS_OK;
}

