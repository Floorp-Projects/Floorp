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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsICharsetAlias.h"
#include "nsXMLEncodingObserver.h"
#include "nsIXMLEncodingService.h"
#include "nsIElementObserver.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupports.h"
#include "nsCRT.h"
#include "nsIParser.h"
#include "pratom.h"
#include "nsCharDetDll.h"
#include "nsIServiceManager.h"
#include "nsObserverBase.h"
#include "nsWeakReference.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//========================================================================== 
//
// Class declaration for the class 
//
//========================================================================== 
class nsXMLEncodingObserver: public nsIElementObserver, 
                             public nsIObserver, 
                             public nsObserverBase,
                             public nsIXMLEncodingService,
                             public nsSupportsWeakReference {

  NS_DECL_ISUPPORTS

public:

  nsXMLEncodingObserver();
  virtual ~nsXMLEncodingObserver();

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
  NS_IMETHOD Notify(PRUint32 aDocumentID, const PRUnichar* aTag, PRUint32 numOfAttributes, 
                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);
  NS_IMETHOD Notify(nsISupports* aDocumentID, const PRUnichar* aTag, const nsDeque* keys, const nsDeque* values)
      { return NS_ERROR_NOT_IMPLEMENTED; }

  /* methode for nsIObserver */
  NS_DECL_NSIOBSERVER

  /* methode for nsIXMLEncodingService */
  NS_IMETHOD Start();
  NS_IMETHOD End();
private:

  NS_IMETHOD Notify(PRUint32 aDocumentID, PRUint32 numOfAttributes, 
                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);

};

//-------------------------------------------------------------------------
nsXMLEncodingObserver::nsXMLEncodingObserver()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(& g_InstanceCount);
}
//-------------------------------------------------------------------------
nsXMLEncodingObserver::~nsXMLEncodingObserver()
{
  PR_AtomicDecrement(& g_InstanceCount);
}

//-------------------------------------------------------------------------
NS_IMPL_ADDREF ( nsXMLEncodingObserver );
NS_IMPL_RELEASE ( nsXMLEncodingObserver );

NS_INTERFACE_MAP_BEGIN(nsXMLEncodingObserver)
	NS_INTERFACE_MAP_ENTRY(nsIElementObserver)
	NS_INTERFACE_MAP_ENTRY(nsIObserver)
	NS_INTERFACE_MAP_ENTRY(nsIXMLEncodingService)
	NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIElementObserver)
NS_INTERFACE_MAP_END

//-------------------------------------------------------------------------
NS_IMETHODIMP_(const char*) nsXMLEncodingObserver::GetTagNameAt(PRUint32 aTagIndex)
{
  if (aTagIndex == 0) {
    return "?XML";
  }else {
    return nsnull;
  }
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsXMLEncodingObserver::Notify(
                     PRUint32 aDocumentID, 
                     const PRUnichar* aTag, 
                     PRUint32 numOfAttributes, 
                     const PRUnichar* nameArray[], 
                     const PRUnichar* valueArray[])
{
    if(0 != nsCRT::strcasecmp(aTag, "?XML")) 
        return NS_ERROR_ILLEGAL_VALUE;
    else
        return Notify(aDocumentID, numOfAttributes, nameArray, valueArray);
}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsXMLEncodingObserver::Notify(
                     PRUint32 aDocumentID, 
                     eHTMLTags aTag, 
                     PRUint32 numOfAttributes, 
                     const PRUnichar* nameArray[], 
                     const PRUnichar* valueArray[])
{
    if(eHTMLTag_meta != aTag) 
        return NS_ERROR_ILLEGAL_VALUE;
    else 
        return Notify(aDocumentID, numOfAttributes, nameArray, valueArray);
}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsXMLEncodingObserver::Notify(
                     PRUint32 aDocumentID, 
                     PRUint32 numOfAttributes, 
                     const PRUnichar* nameArray[], 
                     const PRUnichar* valueArray[])
{

    nsresult res = NS_OK;
    PRUint32 i;

    if(numOfAttributes >= 3)
    {
      PRBool bGotCurrentCharset=PR_FALSE;
      PRBool bGotCurrentCharsetSource = PR_FALSE;
      PRBool bGotEncoding = PR_FALSE;

      nsAutoString currentCharset;    currentCharset.AssignWithConversion("unknown");
      nsAutoString charsetSourceStr;  charsetSourceStr.AssignWithConversion("unknown");
      nsAutoString encoding;          encoding.AssignWithConversion("unknown");

      for(i=0; i < numOfAttributes; i++) 
      {
         if(0==nsCRT::strcmp(nameArray[i], "charset")) 
         {
           bGotCurrentCharset = PR_TRUE;
           currentCharset = valueArray[i];
         } else if(0==nsCRT::strcmp(nameArray[i], "charsetSource")) {
           bGotCurrentCharsetSource = PR_TRUE;
           charsetSourceStr = valueArray[i];
         } else if(0==nsCRT::strcasecmp(nameArray[i], "encoding")) { 
           bGotEncoding = PR_TRUE;
           encoding = valueArray[i];
         }
      }

      // if we cannot find currentCharset or currentCharsetSource
      // return error.
      if( ! (bGotCurrentCharset && bGotCurrentCharsetSource))
      {
         return NS_ERROR_ILLEGAL_VALUE;
      }

      PRInt32 err;
      PRInt32 charsetSourceInt = charsetSourceStr.ToInteger(&err);

      // if we cannot convert the string into nsCharsetSource, return error
      if(NS_FAILED(err))
         return NS_ERROR_ILLEGAL_VALUE;

      nsCharsetSource currentCharsetSource = (nsCharsetSource)charsetSourceInt;

      if(kCharsetFromMetaTag > currentCharsetSource)
      {
           if(! encoding.Equals(currentCharset)) 
           {
               nsICharsetAlias* calias = nsnull;
               res = nsServiceManager::GetService(
                              kCharsetAliasCID,
                              NS_GET_IID(nsICharsetAlias),
                              (nsISupports**) &calias);
               if(NS_SUCCEEDED(res) && (nsnull != calias) ) 
               {
                    PRBool same = PR_FALSE;
                    res = calias->Equals( encoding, currentCharset, &same);
                    if(NS_SUCCEEDED(res) && (! same))
                    {
                          nsAutoString preferred;
                          res = calias->GetPreferred(encoding, preferred);
                          if(NS_SUCCEEDED(res))
                          {
                              const char* charsetInCStr = preferred.ToNewCString();
                              if(nsnull != charsetInCStr) {
                                 res = NotifyWebShell((nsISupports*)aDocumentID, charsetInCStr, kCharsetFromMetaTag );
                                 delete [] (char*)charsetInCStr;
                                 return res;
                              }
                          } // if check for GetPreferred
                    } // if check res for Equals
                    nsServiceManager::ReleaseService(kCharsetAliasCID, calias);
                } // if check res for GetService
            } // if Equals
       } // if 
    } // if 

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsXMLEncodingObserver::Observe(nsISupports*, const PRUnichar*, const PRUnichar*) 
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsXMLEncodingObserver::Start() 
{
    nsresult res = NS_OK;
    nsAutoString xmlTopic; xmlTopic.AssignWithConversion("xmlparser");
    nsIObserverService* anObserverService = nsnull;

    res = nsServiceManager::GetService(NS_OBSERVERSERVICE_CONTRACTID, 
                                       NS_GET_IID(nsIObserverService),
                                       (nsISupports**) &anObserverService);
    if(NS_FAILED(res)) 
        goto done;
     
    res = anObserverService->AddObserver(this, xmlTopic.GetUnicode());

    nsServiceManager::ReleaseService(NS_OBSERVERSERVICE_CONTRACTID, 
                                    anObserverService);
done:
    return res;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsXMLEncodingObserver::End() 
{
    nsresult res = NS_OK;
    nsAutoString xmlTopic; xmlTopic.AssignWithConversion("xmlparser");
    nsIObserverService* anObserverService = nsnull;

    res = nsServiceManager::GetService(NS_OBSERVERSERVICE_CONTRACTID, 
                                       NS_GET_IID(nsIObserverService),
                                       (nsISupports**) &anObserverService);
    if(NS_FAILED(res)) 
        goto done;
     
    res = anObserverService->RemoveObserver(this, xmlTopic.GetUnicode());

    nsServiceManager::ReleaseService(NS_OBSERVERSERVICE_CONTRACTID, 
                                    anObserverService);
done:
    return res;
}
//========================================================================== 

class nsXMLEncodingObserverFactory : public nsIFactory {
   NS_DECL_ISUPPORTS

public:
   nsXMLEncodingObserverFactory() {
     NS_INIT_REFCNT();
     PR_AtomicIncrement(&g_InstanceCount);
   }
   virtual ~nsXMLEncodingObserverFactory() {
     PR_AtomicDecrement(&g_InstanceCount);
   }

   NS_IMETHOD CreateInstance(nsISupports* aDelegate, const nsIID& aIID, void** aResult);
   NS_IMETHOD LockFactory(PRBool aLock);

};

//--------------------------------------------------------------
NS_DEFINE_IID( kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS( nsXMLEncodingObserverFactory , kIFactoryIID);

NS_IMETHODIMP nsXMLEncodingObserverFactory::CreateInstance(
    nsISupports* aDelegate, const nsIID &aIID, void** aResult)
{
  if(NULL == aResult)
        return NS_ERROR_NULL_POINTER;
  if(NULL != aDelegate)
        return NS_ERROR_NO_AGGREGATION;

  *aResult = NULL;

  nsXMLEncodingObserver *inst = new nsXMLEncodingObserver();


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
NS_IMETHODIMP nsXMLEncodingObserverFactory::LockFactory(PRBool aLock)
{
  if(aLock)
     PR_AtomicIncrement( &g_LockCount );
  else
     PR_AtomicDecrement( &g_LockCount );
  return NS_OK;
}

//==============================================================
nsIFactory* NEW_XML_ENCODING_OBSERVER_FACTORY()
{
  return new nsXMLEncodingObserverFactory();
}
