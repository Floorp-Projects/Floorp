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
 */

#define IMPL_NS_IPARSERFILTER

#include "nsICharsetDetectionObserver.h"
#include "nsICharsetDetectionAdaptor.h"
#include "nsDetectionAdaptor.h"
#include "nsICharsetDetector.h"
#include "nsIWebShellServices.h"
#include "plstr.h"
#include "pratom.h"
#include "nsCharDetDll.h"
#ifdef IMPL_NS_IPARSERFILTER
#include "nsIParserFilter.h"
static NS_DEFINE_IID(kIParserFilterIID, NS_IPARSERFILTER_IID);

#endif /* IMPL_NS_IPARSERFILTER */

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

class CToken;

//--------------------------------------------------------------
class nsMyObserver : public nsICharsetDetectionObserver
{
 public:
   NS_DECL_ISUPPORTS

 public:
   nsMyObserver( void )
   {
     NS_INIT_REFCNT();
     PR_AtomicIncrement(& g_InstanceCount);
     mWebShellSvc = nsnull;
     mCommand[0] = '\0';
   }
   virtual  ~nsMyObserver( void )
   {
     NS_IF_RELEASE(mWebShellSvc);
     PR_AtomicDecrement(& g_InstanceCount);
   }


   // Methods to support nsICharsetDetectionAdaptor
   NS_IMETHOD Init(nsIWebShellServices* aWebShellSvc, const char* aCommand);

   // Methods to support nsICharsetDetectionObserver
   NS_IMETHOD Notify(const char* aCharset, nsDetectionConfident aConf);
 private:
     nsIWebShellServices* mWebShellSvc;
     char mCommand[32];
};
//--------------------------------------------------------------
NS_IMETHODIMP nsMyObserver::Notify(
    const char* aCharset, nsDetectionConfident aConf)
{
    nsresult rv = NS_OK;
    rv = mWebShellSvc->SetRendering( PR_FALSE);
    rv = mWebShellSvc->StopDocumentLoad();
    rv = mWebShellSvc->ReloadDocument( aCharset, kCharsetFromAutoDetection,
                       mCommand[0] ?  mCommand : nsnull);
    return NS_OK;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsMyObserver::Init( nsIWebShellServices* aWebShellSvc, 
                                  const char* aCommand)
{
    if(aCommand) {
        PL_strncpy(mCommand, aCommand, 31);
    }
    if(nsnull != aWebShellSvc)
    {
        NS_IF_ADDREF(aWebShellSvc);
        mWebShellSvc = aWebShellSvc;
        return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
}
//--------------------------------------------------------------
NS_IMPL_ISUPPORTS ( nsMyObserver ,nsICharsetDetectionObserver::GetIID());
//--------------------------------------------------------------

class nsDetectionAdaptor : 
#ifdef IMPL_NS_IPARSERFILTER
                           public nsIParserFilter,
#endif /* IMPL_NS_IPARSERFILTER */
                           public nsICharsetDetectionAdaptor
{
 public:
   NS_DECL_ISUPPORTS

 public:
   nsDetectionAdaptor( void );
   virtual  ~nsDetectionAdaptor( void );

   // Methods to support nsICharsetDetectionAdaptor
   NS_IMETHOD Init(nsIWebShellServices* aWebShellSvc, nsICharsetDetector *aDetector, 
                   const char* aCommand=nsnull);
  
   // Methode to suppor nsIParserFilter
   NS_IMETHOD RawBuffer(char * buffer, PRUint32 * buffer_length) ;
   NS_IMETHOD Finish();

   // really don't care the following two, only because they are defined
   // in nsIParserFilter.h
   NS_IMETHOD WillAddToken(CToken & token) { return NS_OK; };
   NS_IMETHOD ProcessTokens( void ) {return NS_OK;};

  private:
     nsICharsetDetector* mDetector;
     PRBool mDontFeedToDetector;
};
//--------------------------------------------------------------
nsDetectionAdaptor::nsDetectionAdaptor( void ) 
{
     NS_INIT_REFCNT();
     PR_AtomicIncrement(& g_InstanceCount);
     mDetector = nsnull;
     mDontFeedToDetector = PR_TRUE;
}
//--------------------------------------------------------------
nsDetectionAdaptor::~nsDetectionAdaptor()
{
     NS_IF_RELEASE(mDetector);
     PR_AtomicDecrement(& g_InstanceCount);
}
//--------------------------------------------------------------
NS_IMPL_ADDREF ( nsDetectionAdaptor );
NS_IMPL_RELEASE ( nsDetectionAdaptor );
//----------------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptor::QueryInterface(REFNSIID aIID, void**aInstancePtr)
{

  if( NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  *aInstancePtr = NULL;

  if( aIID.Equals ( nsICharsetDetectionAdaptor::GetIID() )) {
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
    const char* aCommand)
{
    if((nsnull != aWebShellSvc) && (nsnull != aDetector))
    {
         nsICharsetDetectionObserver* aObserver = nsnull;
         nsresult rv = NS_OK;
         nsMyObserver* mobs = new nsMyObserver();
         if(nsnull == mobs)
            return NS_ERROR_OUT_OF_MEMORY;

         rv = mobs->QueryInterface(nsICharsetDetectionObserver::GetIID(),
                                  (void**) &aObserver);
        
         if(NS_SUCCEEDED(rv)) {
            rv = mobs->Init(aWebShellSvc, aCommand);
            if(NS_SUCCEEDED(rv)) {
               rv = aDetector->Init(aObserver);
            }

            NS_IF_RELEASE(aObserver);

            if(NS_FAILED(rv))
                 return rv;

            NS_IF_ADDREF(aDetector);
            mDetector = aDetector;

            mDontFeedToDetector = PR_FALSE;
            return NS_OK;
         } else {
            delete mobs;
	 }
    }
    return NS_ERROR_ILLEGAL_VALUE;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptor::RawBuffer
    (char * buffer, PRUint32 * buffer_length) 
{
    if((mDontFeedToDetector) || (nsnull == mDetector))
       return NS_OK;
    nsresult rv = NS_OK;
    rv = mDetector->DoIt((const char*)buffer, *buffer_length, &mDontFeedToDetector);
    return NS_OK;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptor::Finish()
{
    if((mDontFeedToDetector) || (nsnull == mDetector))
       return NS_OK;
    nsresult rv = NS_OK;
    rv = mDetector->Done();

    NS_IF_RELEASE(mDetector); // need this line so no cycle reference

    return NS_OK;
}
//--------------------------------------------------------------
class nsDetectionAdaptorFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsDetectionAdaptorFactory() {
     NS_INIT_REFCNT();
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~nsDetectionAdaptorFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);

};

//--------------------------------------------------------------
NS_DEFINE_IID( kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS( nsDetectionAdaptorFactory , kIFactoryIID);

//--------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptorFactory::CreateInstance(
    nsISupports* aDelegate, const nsIID &aIID, void** aResult)
{
  if(NULL == aResult)
        return NS_ERROR_NULL_POINTER;
  if(NULL != aDelegate)
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;

  nsDetectionAdaptor *inst = new nsDetectionAdaptor();


  if(NULL == inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res =inst->QueryInterface(aIID, aResult);
  if(NS_FAILED(res)) {
     delete inst;
  }

  return res;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsDetectionAdaptorFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}

//==============================================================
nsIFactory* NEW_DETECTION_ADAPTOR_FACTORY()
{
  return new nsDetectionAdaptorFactory();
}
