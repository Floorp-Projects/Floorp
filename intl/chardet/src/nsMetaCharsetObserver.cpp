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

  /* methode for nsIObserver */
  NS_DECL_NSIOBSERVER

  /* methode for nsIMetaCharsetService */
  NS_IMETHOD Start();
  NS_IMETHOD End();
private:

  NS_IMETHOD Notify(PRUint32 aDocumentID, PRUint32 numOfAttributes, 
                    const PRUnichar* nameArray[], const PRUnichar* valueArray[]);
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
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Notify(
                     PRUint32 aDocumentID, 
                     PRUint32 numOfAttributes, 
                     const PRUnichar* nameArray[], 
                     const PRUnichar* valueArray[])
{
#if 1 // perfor rewrite fix bug 17409
    nsresult res=NS_OK;
#ifdef DEBUG
    PRUnichar Uxcommand[]={'X','_','C','O','M','M','A','N','D','\0'};
    PRUnichar UcharsetSource[]=
         {'c','h','a','r','s','e','t','S','o','u','r','c','e','\0'};
    PRUnichar Ucharset[]={'c','h','a','r','s','e','t','\0'};
    NS_ASSERTION(numOfAttributes >= 3, "should have at least 3 private attribute");
    NS_ASSERTION(0==nsCRT::strcmp(Uxcommand,nameArray[numOfAttributes-1]),
                 "last name should be 'X_COMMAND'" );
    NS_ASSERTION(0==nsCRT::strcmp(UcharsetSource,nameArray[numOfAttributes-2]),
                 "2nd last name should be 'charsetSource'" );
    NS_ASSERTION(0==nsCRT::strcmp(Ucharset,nameArray[numOfAttributes-3]),
                 "3rd last name should be 'charset'" );
#endif
    NS_ASSERTION(mAlias, "Didn't get nsICharsetAlias in constructor");

    if(nsnull == mAlias)
      return NS_ERROR_ABORT;

    // we need at least 5 - HTTP-EQUIV, CONTENT and 3 private
    if(numOfAttributes >= 5 ) 
    {
      const PRUnichar *charset = valueArray[numOfAttributes-3];
      const PRUnichar *source =  valueArray[numOfAttributes-2];
      PRInt32 err;
      nsAutoString srcStr(source);
      nsCharsetSource  src = (nsCharsetSource) srcStr.ToInteger(&err);
      // if we cannot convert the string into nsCharsetSource, return error
      NS_ASSERTION(NS_SUCCEEDED(err), "cannot get charset source");
      if(NS_FAILED(err))
          return NS_ERROR_ILLEGAL_VALUE;

      if(kCharsetFromMetaTag <= src)
          return NS_OK; // current charset have higher priority. do bother to do the following

      PRUint32 i;
      const PRUnichar *httpEquivValue=nsnull;
      const PRUnichar *contentValue=nsnull;
      for(i=0;i<(numOfAttributes-3);i++)
      {
        if(0 == nsCRT::strcasecmp(nameArray[i], "HTTP-EQUIV"))
              httpEquivValue=valueArray[i];
        else if(0 == nsCRT::strcasecmp(nameArray[i], "content"))
              contentValue=valueArray[i];
      }
      static nsAutoString contenttype("Content-Type");
      static nsAutoString texthtml("text/html");
      if((nsnull != httpEquivValue) && 
         (nsnull != contentValue) && 
         ((0==nsCRT::strcasecmp(httpEquivValue,contenttype.GetUnicode())) ||
          ((((httpEquivValue[0]=='\'') &&
             (httpEquivValue[contenttype.Length()+1]=='\'')) ||
            ((httpEquivValue[0]=='\"') &&
             (httpEquivValue[contenttype.Length()+1]=='\"'))) && 
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
             if(! newCharset.EqualsIgnoreCase(charset)) 
             {
                 PRBool same = PR_FALSE;
                 res = mAlias->Equals( newCharset, charset , &same);
                 if(NS_SUCCEEDED(res) && (! same))
                 {
                     nsAutoString preferred;
                     res = mAlias->GetPreferred(newCharset, preferred);
                     if(NS_SUCCEEDED(res))
                     {
                        const char* newCharsetStr = preferred.ToNewCString();
                        NS_ASSERTION(newCharsetStr, "out of memory");
                        if(nsnull != newCharsetStr) 
                        {
                           res = NotifyWebShell(aDocumentID, newCharsetStr, 
                                      kCharsetFromMetaTag);
                           delete [] (char*)newCharsetStr;
                           return res;
                        } // if(nsnull...)
                        if(newCharsetStr)
                           delete [] (char*)newCharsetStr;
                     } // if(NS_SUCCEEDED(res)
                 }
             } // if EqualIgnoreCase 
         } // if (kNotFound != start)	
      } // if
    }
    return NS_OK;
#else // old code

    nsresult res = NS_OK;
    PRUint32 i;
    char command[256];
    command[0]='\0';

    // Only process if we get the HTTP-EQUIV=Content-Type in meta
    // We totaly need 4 attributes
    //   HTTP-EQUIV
    //   CONTENT
    //   currentCharset            - pseudo attribute fake by parser
    //   currentCharsetSource      - pseudo attribute fake by parser
    //   X_COMMAND                 - pseudo attribute fake by parser

    if((numOfAttributes >= 4) && 
       (0 == nsCRT::strcasecmp(nameArray[0], "HTTP-EQUIV")) &&
       (0 == nsCRT::strncasecmp(((('\'' == valueArray[0][0]) || ('\"' == valueArray[0][0]))
                                 ? (valueArray[0]+1) 
                                 : valueArray[0]),
                                "Content-Type", 
                                12)))
    {
      PRBool bGotCharset = PR_FALSE;
      PRBool bGotCharsetSource = PR_FALSE;
      nsAutoString currentCharset("unknown");
      nsAutoString charsetSourceStr("unknown");

      for(i=0; i < numOfAttributes; i++) 
      {
         if(0==nsCRT::strcmp(nameArray[i], "charset")) 
         {
           bGotCharset = PR_TRUE;
           currentCharset = valueArray[i];
         } else if(0==nsCRT::strcmp(nameArray[i], "charsetSource")) {
           bGotCharsetSource = PR_TRUE;
           charsetSourceStr = valueArray[i];
         } else if(0==nsCRT::strcmp(nameArray[i], "X_COMMAND")) {
           nsAutoString tmp(valueArray[i]);
           tmp.ToCString(command, 256);
         }
      }

      // if we cannot find currentCharset or currentCharsetSource
      // return error.
      if(! ( bGotCharsetSource && bGotCharset ))
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
          for(i=0; i < numOfAttributes; i++) 
          {
              if (0==nsCRT::strcasecmp(nameArray[i],"CONTENT")) 
              {
                 const PRUnichar *attr = valueArray[i] ;
                 if(('\"' == attr[0]) || ('\'' == attr[0]))
                     attr++;
        
                 nsAutoString content(attr);
                 nsAutoString type;
        
                 content.Left(type, 9); // length of "text/html" == 9
                 if(type.EqualsIgnoreCase("text/html")) 
                 {
                    PRInt32 charsetValueStart = content.RFind("charset=", PR_TRUE ) ;
                    if(kNotFound != charsetValueStart) 
                    {	
                         charsetValueStart += 8; // 8 = "charset=".length 
                         PRInt32 charsetValueEnd = content.FindCharInSet("\'\";", charsetValueStart  );
                         if(kNotFound == charsetValueEnd ) 
                             charsetValueEnd = content.Length();
                         nsAutoString theCharset;
                         content.Mid(theCharset, charsetValueStart, charsetValueEnd - charsetValueStart);
                         if(! theCharset.Equals(currentCharset)) 
                         {
                             NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &res);
                             if(NS_SUCCEEDED(res) && (nsnull != calias) ) 
                             {
                                  PRBool same = PR_FALSE;
                                  res = calias->Equals( theCharset, currentCharset, &same);
                                  if(NS_SUCCEEDED(res) && (! same))
                                  {
                                        nsAutoString preferred;
                                        res = calias->GetPreferred(theCharset, preferred);
                                        if(NS_SUCCEEDED(res))
                                        {
                                            const char* charsetInCStr = preferred.ToNewCString();
                                            if(nsnull != charsetInCStr) {
                                               res = NotifyWebShell(aDocumentID, charsetInCStr, kCharsetFromMetaTag , 
                                                                    command[0]?command:nsnull);
                                               delete [] (char*)charsetInCStr;
                                               return res;
                                            }
                                        } // if check for GetPreferred
                                  } // if check res for Equals
                              } // if check res for GetService
                          } // if Equals
                    }  // if check  charset=
                 } // if check text/html
                 break;
              } // if check CONTENT
          } // for ( numOfAttributes )
      } // if check nsCharsetSource
    } // if 
    return NS_OK;
#endif
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
    nsAutoString htmlTopic("htmlparser");

    NS_WITH_SERVICE(nsIObserverService, anObserverService, NS_OBSERVERSERVICE_PROGID, &res);
    if (NS_FAILED(res)) return res;
     
    return anObserverService->AddObserver(this, htmlTopic.GetUnicode());
}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::End() 
{
    nsresult res = NS_OK;
    nsAutoString htmlTopic("htmlparser");

    NS_WITH_SERVICE(nsIObserverService, anObserverService, NS_OBSERVERSERVICE_PROGID, &res);
    if (NS_FAILED(res)) return res;
     
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
