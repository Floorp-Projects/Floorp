/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#define IMPL_NS_IPARSERFILTER

#include "nsString.h"
#include "plstr.h"
#include "pratom.h"
#include "nsCharDetDll.h"
#include "nsIParser.h"
#include "nsIDocument.h"
#include "nsDetectionAdaptor.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//--------------------------------------------------------------
NS_IMETHODIMP nsMyObserver::Notify(
    const char* aCharset, nsDetectionConfident aConf)
{
    nsresult rv = NS_OK;
    if(!mCharset.EqualsWithConversion(aCharset)) {
      if(mNotifyByReload) {
        rv = mWebShellSvc->SetRendering( PR_FALSE);
        rv = mWebShellSvc->StopDocumentLoad();
        rv = mWebShellSvc->ReloadDocument(aCharset, kCharsetFromAutoDetection);
      } else {
        nsAutoString newcharset; newcharset.AssignWithConversion(aCharset);
        if(mWeakRefDocument) {
             mWeakRefDocument->SetDocumentCharacterSet(newcharset);
        }
        if(mWeakRefParser) {
             mWeakRefParser->SetDocumentCharset(newcharset, kCharsetFromAutoDetection);
        }
      }
    }
    return NS_OK;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsMyObserver::Init( nsIWebShellServices* aWebShellSvc, 
                   nsIDocument* aDocument,
                   nsIParser* aParser,
                   const PRUnichar* aCharset,
                   const char* aCommand)
{
    if(aCommand) {
        mCommand = aCommand;
    }
    if(aCharset) {
        mCharset = aCharset;
    }
    if(aDocument) {
        mWeakRefDocument = aDocument;
    }
    if(aParser) {
        mWeakRefParser = aParser;
    }
    if(nsnull != aWebShellSvc)
    {
        mWebShellSvc = aWebShellSvc;
        return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
}
//--------------------------------------------------------------
NS_IMPL_ISUPPORTS1 ( nsMyObserver ,nsICharsetDetectionObserver);

//--------------------------------------------------------------
nsDetectionAdaptor::nsDetectionAdaptor( void ) 
{
     NS_INIT_REFCNT();
     PR_AtomicIncrement(& g_InstanceCount);
     mDetector = nsnull;
     mDontFeedToDetector = PR_TRUE;
     mObserver = nsnull;
}
//--------------------------------------------------------------
nsDetectionAdaptor::~nsDetectionAdaptor()
{
     PR_AtomicDecrement(& g_InstanceCount);
}
//--------------------------------------------------------------
NS_IMPL_ADDREF ( nsDetectionAdaptor );
NS_IMPL_RELEASE ( nsDetectionAdaptor );
//----------------------------------------------------------------------
// here: can't use NS_IMPL_QUERYINTERFACE due to the #ifdef
//----------------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptor::QueryInterface(REFNSIID aIID, void**aInstancePtr)
{

  if( NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  *aInstancePtr = NULL;

  if( aIID.Equals ( NS_GET_IID(nsICharsetDetectionAdaptor) )) {
    *aInstancePtr = (void*) ((nsICharsetDetectionAdaptor*) this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
#ifdef IMPL_NS_IPARSERFILTER
  if( aIID.Equals ( kIParserFilterIID)) {
    *aInstancePtr = (void*) ((nsIParserFilter*) this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
#endif /* IMPL_NS_IPARSERFILTER */

  if( aIID.Equals ( kISupportsIID )) {
    *aInstancePtr = (void*) ( this );
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

//--------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptor::Init(
    nsIWebShellServices* aWebShellSvc, nsICharsetDetector *aDetector,
    nsIDocument* aDocument, nsIParser* aParser, const PRUnichar* aCharset,
    const char* aCommand)
{
    if((nsnull != aWebShellSvc) && (nsnull != aDetector) && (nsnull != aCharset))
    {
      nsICharsetDetectionObserver* aObserver = nsnull;
      nsresult rv = NS_OK;
      mObserver = new nsMyObserver(); // weak ref to it, release by charset detector
      if(nsnull == mObserver)
         return NS_ERROR_OUT_OF_MEMORY;

      rv = mObserver->QueryInterface(NS_GET_IID(nsICharsetDetectionObserver),
                               (void**) &aObserver);
     
      if(NS_SUCCEEDED(rv)) {
         rv = mObserver->Init(aWebShellSvc, aDocument, 
              aParser, aCharset, aCommand);
         if(NS_SUCCEEDED(rv)) {
            rv = aDetector->Init(aObserver);
         }

         if(NS_FAILED(rv))
              return rv;

         mDetector = aDetector;

         mDontFeedToDetector = PR_FALSE;
         return NS_OK;
      } 
    }
    return NS_ERROR_ILLEGAL_VALUE;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptor::RawBuffer
    (const char * buffer, PRUint32 * buffer_length) 
{
    if((mDontFeedToDetector) || (nsnull == mDetector))
       return NS_OK;
    nsresult rv = NS_OK;
    rv = mDetector->DoIt((const char*)buffer, *buffer_length, &mDontFeedToDetector);
    if(mObserver) 
       mObserver->SetNotifyByReload(PR_TRUE);

    return NS_OK;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptor::Finish()
{
    if((mDontFeedToDetector) || (nsnull == mDetector))
       return NS_OK;
    nsresult rv = NS_OK;
    rv = mDetector->Done();

    return NS_OK;
}

