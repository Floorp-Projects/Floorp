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
 */
#include "nsDeque.h"
#include "nsICharsetAlias.h"
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
#include "nsObserverBase.h"
#include "nsWeakReference.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//========================================================================== 
//
// Class declaration for the class 
//
//========================================================================== 
class nsMetaCharsetObserver: public nsIElementObserver, 
                             public nsIObserver, 
                             public nsObserverBase,
                             public nsIMetaCharsetService,
                             public nsSupportsWeakReference {

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
  NS_IMETHOD Notify(PRUint32 aDocumentID, const PRUnichar* aTag, PRUint32 numOfAttributes, 
                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);

  NS_IMETHOD Notify(nsISupports* aDocumentID, const PRUnichar* aTag, const nsDeque* keys, const nsDeque* values);

  /* methode for nsIObserver */
  NS_DECL_NSIOBSERVER

  /* methode for nsIMetaCharsetService */
  NS_IMETHOD Start();
  NS_IMETHOD End();

private:

  NS_IMETHOD Notify(PRUint32 aDocumentID, PRUint32 numOfAttributes, 
                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);

  NS_IMETHOD Notify(nsISupports* aDocumentID, const nsDeque* keys, const nsDeque* values);

  nsICharsetAlias *mAlias;

};

//-------------------------------------------------------------------------
nsMetaCharsetObserver::nsMetaCharsetObserver()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(& g_InstanceCount);
  nsresult res;
  mAlias = nsnull;
  NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &res);
  if(NS_SUCCEEDED(res)) {
     mAlias = calias;
     NS_ADDREF(mAlias);
  }
}
//-------------------------------------------------------------------------
nsMetaCharsetObserver::~nsMetaCharsetObserver()
{
  // should we release mAlias
  PR_AtomicDecrement(& g_InstanceCount);
  NS_IF_RELEASE(mAlias);
}

//-------------------------------------------------------------------------
NS_IMPL_ADDREF ( nsMetaCharsetObserver );
NS_IMPL_RELEASE ( nsMetaCharsetObserver );

NS_INTERFACE_MAP_BEGIN(nsMetaCharsetObserver)
	NS_INTERFACE_MAP_ENTRY(nsIElementObserver)
	NS_INTERFACE_MAP_ENTRY(nsIObserver)
	NS_INTERFACE_MAP_ENTRY(nsIMetaCharsetService)
	NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIElementObserver)
NS_INTERFACE_MAP_END

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
                     const PRUnichar* aTag, 
                     PRUint32 numOfAttributes, 
                     const PRUnichar* nameArray[], 
                     const PRUnichar* valueArray[])
{
    if(0 != nsCRT::strcasecmp(aTag, "META")) 
        return NS_ERROR_ILLEGAL_VALUE;
    else
        return Notify(aDocumentID, numOfAttributes, nameArray, valueArray);
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
    else 
        return Notify(aDocumentID, numOfAttributes, nameArray, valueArray);
}

NS_IMETHODIMP nsMetaCharsetObserver::Notify(
                     PRUint32 aDocumentID, 
                     PRUint32 numOfAttributes, 
                     const PRUnichar* nameArray[], 
                     const PRUnichar* valueArray[])
{
   nsDeque keys(0);
   nsDeque values(0);
   PRUint32 i;
   for(i=0;i<numOfAttributes;i++)
   {
       keys.Push((void*)nameArray[i]);
       values.Push((void*)valueArray[i]);
   }
   return Notify((nsISupports*)aDocumentID, &keys, &values);
}
NS_IMETHODIMP nsMetaCharsetObserver::Notify(
                     nsISupports* aDocumentID, 
                     const PRUnichar* aTag, 
                     const nsDeque* keys, const nsDeque* values)
{
    if(0 != nsCRT::strcasecmp(aTag, "META")) 
        return NS_ERROR_ILLEGAL_VALUE;
    else
        return Notify(aDocumentID, keys, values);
}
NS_IMETHODIMP nsMetaCharsetObserver::Notify(
                    nsISupports* aDocumentID, 
                    const nsDeque* keys, const nsDeque* values)
{
    NS_PRECONDITION(keys!=nsnull && values!=nsnull,"Need key-value pair");

    PRInt32 numOfAttributes = keys->GetSize();
    NS_ASSERTION( numOfAttributes == values->GetSize(), "size mismatch");
    nsresult res=NS_OK;
#ifdef DEBUG
    PRUnichar Uxcommand[]={'X','_','C','O','M','M','A','N','D','\0'};
    PRUnichar UcharsetSource[]=
         {'c','h','a','r','s','e','t','S','o','u','r','c','e','\0'};
    PRUnichar Ucharset[]={'c','h','a','r','s','e','t','\0'};
    NS_ASSERTION(numOfAttributes >= 3, "should have at least 3 private attribute");
    NS_ASSERTION(0==nsCRT::strcmp(Uxcommand,(const PRUnichar*)keys->ObjectAt(numOfAttributes-1)),
                 "last name should be 'X_COMMAND'" );
    NS_ASSERTION(0==nsCRT::strcmp(UcharsetSource,(const PRUnichar*)keys->ObjectAt(numOfAttributes-2)),
                 "2nd last name should be 'charsetSource'" );
    NS_ASSERTION(0==nsCRT::strcmp(Ucharset,(const PRUnichar*)keys->ObjectAt(numOfAttributes-3)),
                 "3rd last name should be 'charset'" );
#endif
    NS_ASSERTION(mAlias, "Didn't get nsICharsetAlias in constructor");

    if(nsnull == mAlias)
      return NS_ERROR_ABORT;

    // we need at least 5 - HTTP-EQUIV, CONTENT and 3 private
    if(numOfAttributes >= 5 ) 
    {
      const PRUnichar *charset = (const PRUnichar*)values->ObjectAt(numOfAttributes-3);
      const PRUnichar *source = (const PRUnichar*)values->ObjectAt(numOfAttributes-2);
      PRInt32 err;
      nsAutoString srcStr(source);
      nsCharsetSource  src = (nsCharsetSource) srcStr.ToInteger(&err);
      // if we cannot convert the string into nsCharsetSource, return error
      NS_ASSERTION(NS_SUCCEEDED(err), "cannot get charset source");
      if(NS_FAILED(err))
          return NS_ERROR_ILLEGAL_VALUE;

      if(kCharsetFromMetaTag <= src)
          return NS_OK; // current charset have higher priority. do bother to do the following

      PRInt32 i;
      const PRUnichar *httpEquivValue=nsnull;
      const PRUnichar *contentValue=nsnull;
      for(i=0;i<(numOfAttributes-3);i++)
      {
        if(0 == nsCRT::strcasecmp((const PRUnichar*)keys->ObjectAt(i), "HTTP-EQUIV"))
              httpEquivValue=(const PRUnichar*)values->ObjectAt(i);
        else if(0 == nsCRT::strcasecmp((const PRUnichar*)keys->ObjectAt(i), "content"))
              contentValue=(const PRUnichar*)values->ObjectAt(i);
      }
      static nsAutoString contenttype = NS_ConvertToString("Content-Type");
      static nsAutoString texthtml = NS_ConvertToString("text/html");

      PRInt32 lastIdx =0;
      if(nsnull != httpEquivValue)
         lastIdx = nsCRT::strlen(httpEquivValue)-1;

      if((nsnull != httpEquivValue) && 
         (nsnull != contentValue) && 
         ((0==nsCRT::strcasecmp(httpEquivValue,contenttype.GetUnicode())) ||
          ((((httpEquivValue[0]=='\'') &&
             (httpEquivValue[lastIdx]=='\'')) ||
            ((httpEquivValue[0]=='\"') &&
             (httpEquivValue[lastIdx]=='\"'))) && 
           (0==nsCRT::strncasecmp(httpEquivValue+1,
                      contenttype.GetUnicode(),
                      contenttype.Length()))
          )) &&
         ((0==nsCRT::strcasecmp(contentValue,texthtml.GetUnicode())) ||
          (((contentValue[0]=='\'') ||
            (contentValue[0]=='\"'))&&
           (0==nsCRT::strncasecmp(contentValue+1,
                      texthtml.GetUnicode(),
                      texthtml.Length()))
          ))
        )
      {
         nsAutoString contentPart1(contentValue+10); // after "text/html;"
         PRInt32 start = contentPart1.RFind("charset=", PR_TRUE ) ;
         if(kNotFound != start) 
         {	
             start += 8; // 8 = "charset=".length 
             PRInt32 end = contentPart1.FindCharInSet("\'\";", start  );
             if(kNotFound == end ) 
                end = contentPart1.Length();
             nsAutoString newCharset;
             NS_ASSERTION(end>=start, "wrong index");
             contentPart1.Mid(newCharset, start, end - start);
             
             nsAutoString charsetString(charset);
             if(! newCharset.EqualsIgnoreCase(charsetString)) 
             {
                 PRBool same = PR_FALSE;
                 res = mAlias->Equals( newCharset, charsetString , &same);
                 if(NS_SUCCEEDED(res) && (! same))
                 {
                     nsAutoString preferred;
                     res = mAlias->GetPreferred(newCharset, preferred);
                     if(NS_SUCCEEDED(res))
                     {
                        res = NotifyWebShell(aDocumentID, NS_ConvertUCS2toUTF8(preferred.GetUnicode()), 
                                   kCharsetFromMetaTag);
                     } // if(NS_SUCCEEDED(res)
                 }
             } // if EqualIgnoreCase 
         } // if (kNotFound != start)	
      } // if
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Observe(nsISupports*, const PRUnichar*, const PRUnichar*)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}                                                  
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Start() 
{
    nsresult res = NS_OK;

    NS_WITH_SERVICE(nsIObserverService, anObserverService, NS_OBSERVERSERVICE_CONTRACTID, &res);
    if (NS_FAILED(res)) return res;

    nsAutoString htmlTopic; htmlTopic.AssignWithConversion(kHTMLTextContentType);    
    return anObserverService->AddObserver(this, htmlTopic.GetUnicode());
}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::End() 
{
    nsresult res = NS_OK;

    NS_WITH_SERVICE(nsIObserverService, anObserverService, NS_OBSERVERSERVICE_CONTRACTID, &res);
    if (NS_FAILED(res)) return res;
     
    nsAutoString htmlTopic; htmlTopic.AssignWithConversion(kHTMLTextContentType);
    return anObserverService->RemoveObserver(this, htmlTopic.GetUnicode());
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
