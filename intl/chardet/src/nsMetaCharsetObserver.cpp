/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-dd
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

#define NS_IMPL_IDS
#include "nsICharsetAlias.h"
#undef NS_IMPL_IDS

#include "nsMetaCharsetObserver.h"
#include "nsIMetaCharsetService.h"
#include "nsIElementObserver.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupports.h"
#include "nsCRT.h"
#include "nsIParser.h"
#include "pratom.h"
#include "nsCharDetDll.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kIElementObserverIID, NS_IELEMENTOBSERVER_IID);
static NS_DEFINE_IID(kIObserverIID, NS_IOBSERVER_IID);
static NS_DEFINE_IID(kObserverServiceCID, NS_OBSERVERSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIMetaCharsetServiceIID, NS_IMETA_CHARSET_SERVICE_IID);

//========================================================================== 
//
// Class declaration for the class 
//
//========================================================================== 
class nsMetaCharsetObserver: public nsIElementObserver, 
                             public nsIObserver, 
                             public nsIMetaCharsetService {

  NS_DECL_ISUPPORTS

public:

  nsMetaCharsetObserver();
  virtual ~nsMetaCharsetObserver();

  /* methode for nsIElementObserver */
  /*
   *   This method return the tag which the observer care about
   */
  NS_IMETHOD_(const char*)GetTagNameAt(PRUint32 aTagIndex);

  /*
   *   Subject call observer when the parser hit the tag
   *   @param aDocumentID- ID of the document
   *   @param aTag- the tag
   *   @param numOfAttributes - number of attributes
   *   @param nameArray - array of name. 
   *   @param valueArray - array of value
   */
  NS_IMETHOD Notify(PRUint32 aDocumentID, eHTMLTags aTag, PRUint32 numOfAttributes, 
                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);

  /* methode for nsIObserver */
  NS_IMETHOD Notify(nsISupports** result);

  /* methode for nsIMetaCharsetService */
  NS_IMETHOD Start();
  NS_IMETHOD End();
private:
  nsIObserver* mHack;
};

//-------------------------------------------------------------------------
nsMetaCharsetObserver::nsMetaCharsetObserver()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(& g_InstanceCount);
  mHack = this;
}
//-------------------------------------------------------------------------
nsMetaCharsetObserver::~nsMetaCharsetObserver()
{
  PR_AtomicDecrement(& g_InstanceCount);
}

//-------------------------------------------------------------------------
NS_IMPL_ADDREF ( nsMetaCharsetObserver );
NS_IMPL_RELEASE ( nsMetaCharsetObserver );

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{

  if( NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  *aInstancePtr = NULL;

  if( aIID.Equals ( kIElementObserverIID )) {
    *aInstancePtr = (void*) ((nsIElementObserver*) this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if( aIID.Equals ( kIObserverIID )) {
    *aInstancePtr = (void*) ((nsIObserver*) this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if( aIID.Equals ( kIMetaCharsetServiceIID )) {
    *aInstancePtr = (void*) ((nsIMetaCharsetService*) this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if( aIID.Equals ( kISupportsIID )) {
    *aInstancePtr = (void*) (this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP_(const char*) nsMetaCharsetObserver::GetTagNameAt(PRUint32 aTagIndex)
{
  if (aTagIndex == 0) {
    return "META";
  }else {
    return nsnull;
  }
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Notify(
                     PRUint32 aDocumentID, 
                     eHTMLTags aTag, 
                     PRUint32 numOfAttributes, 
                     const PRUnichar* nameArray[], 
                     const PRUnichar* valueArray[])
{
    if(eHTMLTag_meta != aTag) 
        return NS_ERROR_ILLEGAL_VALUE;

    nsresult res = NS_OK;

    // Only process if we get the HTTP-EQUIV=Content-Type in meta
    // We totaly need 4 attributes
    //   HTTP-EQUIV
    //   CONTENT
    //   currentCharset            - pseudo attribute fake by parser
    //   currentCharsetSource      - pseudo attribute fake by parser

    if((numOfAttributes >= 4) && 
       (0 == nsCRT::strcasecmp(nameArray[0], "HTTP-EQUIV")) &&
       (0 == nsCRT::strcasecmp(valueArray[0], "Content-Type")))
    {
      nsAutoString currentCharset("unknown");
      nsAutoString charsetSourceStr("unknown");

      for(PRUint32 i=0; i < numOfAttributes; i++) 
      {
         nsAutoString charset("charset");
         nsAutoString charsetSource("charsetSource");
         if(0==nsCRT::strcmp(nameArray[i], "charset")) 
         {
           currentCharset = valueArray[i];
         } else if(0==nsCRT::strcmp(nameArray[i], "charsetSource")) {
           charsetSourceStr = valueArray[i];
         }
      }

      // if we cannot find currentCharset or currentCharsetSource
      // return error.
      if( currentCharset.Equals("unknown") ||
          charsetSourceStr.Equals("unknown") )
      {
         return NS_ERROR_ILLEGAL_VALUE;
      }

      PRInt32 err;
      PRInt32 charsetSourceInt = charsetSourceStr.ToInteger(&err);

      // if we cannot convert the string into nsCharsetSource, return error
      if(NS_FAILED(err))
         return NS_ERROR_ILLEGAL_VALUE;

      nsCharsetSource currentCharsetSource = (nsCharsetSource)charsetSourceInt;

      if (0==nsCRT::strcasecmp(nameArray[1],"CONTENT")) 
      {
         nsAutoString content(valueArray[2]);
         nsAutoString type;
         content.Left(type, 9); // length of "text/html" == 9
         if(type.EqualsIgnoreCase("text/html")) 
         {
            PRInt32 charsetValueStart = content.RFind("charset=", PR_TRUE ) ;
            if(kNotFound != charsetValueStart) {	
                  charsetValueStart += 8; // 8 = "charset=".length 
                  PRInt32 charsetValueEnd = content.FindCharInSet("\'\";", charsetValueStart  );
                  if(kNotFound == charsetValueEnd ) 
                     charsetValueEnd = content.Length();
                  nsAutoString theCharset;
                  content.Mid(theCharset, charsetValueStart, charsetValueEnd - charsetValueStart);

                  if(kCharsetFromMetaTag > currentCharsetSource)
                  {
                     nsICharsetAlias* calias = nsnull;
                     res = nsServiceManager::GetService(
                                kCharsetAliasCID,
                                kICharsetAliasIID,
                                (nsISupports**) &calias);
                     if(NS_SUCCEEDED(res) && (nsnull != calias) ) {
                         PRBool same = PR_FALSE;
                         res = calias->Equals( theCharset, currentCharset, &same);
                         if(NS_SUCCEEDED(res) && (! same))
                         {
                            nsAutoString preferred;
                            res = calias->GetPreferred(theCharset, preferred);
                            if(NS_SUCCEEDED(res))
                            {
                               // XXX
                               // ask nsWebShellProxy to load docuement
                               // where the docuemtn ID is euqal to aDocuemntID
                               // Charset equal to preferred
                               // and charsetSource is kCharetFromMetaTag        
                            }
                         }
                         nsServiceManager::ReleaseService(kCharsetAliasCID, calias);
                     }
                  }
                  
            } // if
          
         } // if
      } // if
    } // if
    return res;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Notify(nsISupports** result) 
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Start() 
{
    nsresult res = NS_OK;
    nsAutoString htmlTopic("htmlparser");
    nsIObserverService* anObserverService = nsnull;

    res = nsServiceManager::GetService(kObserverServiceCID, 
                                       nsIObserverService::GetIID(),
                                       (nsISupports**) &anObserverService);
    if(NS_FAILED(res)) 
        goto done;
     
    res = anObserverService->AddObserver(&mHack, &htmlTopic);

    nsServiceManager::ReleaseService(kObserverServiceCID, 
                                    anObserverService);
done:
    return res;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::End() 
{
    nsresult res = NS_OK;
    nsAutoString htmlTopic("htmlparser");
    nsIObserverService* anObserverService = nsnull;

    res = nsServiceManager::GetService(kObserverServiceCID, 
                                       nsIObserverService::GetIID(),
                                       (nsISupports**) &anObserverService);
    if(NS_FAILED(res)) 
        goto done;
     
    res = anObserverService->RemoveObserver(&mHack, &htmlTopic);

    nsServiceManager::ReleaseService(kObserverServiceCID, 
                                    anObserverService);
done:
    return res;
}
//========================================================================== 

class nsMetaCharsetObserverFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsMetaCharsetObserverFactory() {
     NS_INIT_REFCNT();
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~nsMetaCharsetObserverFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);

};

//--------------------------------------------------------------
NS_DEFINE_IID( kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS( nsMetaCharsetObserverFactory , kIFactoryIID);

NS_IMETHODIMP nsMetaCharsetObserverFactory::CreateInstance(
    nsISupports* aDelegate, const nsIID &aIID, void** aResult)
{
  if(NULL == aResult)
        return NS_ERROR_NULL_POINTER;
  if(NULL != aDelegate)
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;

  nsMetaCharsetObserver *inst = new nsMetaCharsetObserver();


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
NS_IMETHODIMP nsMetaCharsetObserverFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}

//==============================================================
nsIFactory* NEW_META_CHARSET_OBSERVER_FACTORY()
{
  return new nsMetaCharsetObserverFactory();
}
