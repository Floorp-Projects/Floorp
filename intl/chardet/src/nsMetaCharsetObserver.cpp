/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
#include "nsIAppStartupNotifier.h"
#include "pratom.h"
#include "nsCharDetDll.h"
#include "nsIServiceManager.h"
#include "nsObserverBase.h"
#include "nsWeakReference.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//-------------------------------------------------------------------------
nsMetaCharsetObserver::nsMetaCharsetObserver()
{
  NS_INIT_REFCNT();
  bMetaCharsetObserverStarted = PR_FALSE;
  nsresult res;
  mAlias = nsnull;
  nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &res));
  if(NS_SUCCEEDED(res)) {
     mAlias = calias;
  }
}
//-------------------------------------------------------------------------
nsMetaCharsetObserver::~nsMetaCharsetObserver()
{
  // should we release mAlias
  if (bMetaCharsetObserverStarted == PR_TRUE)  {
    // call to end the ObserverService
    End();
  }
}

//-------------------------------------------------------------------------
NS_IMPL_ADDREF ( nsMetaCharsetObserver );
NS_IMPL_RELEASE ( nsMetaCharsetObserver );

// Use the new scheme
NS_IMPL_QUERY_INTERFACE4(nsMetaCharsetObserver, 
                         nsIElementObserver, 
                         nsIObserver, 
                         nsIMetaCharsetService, 
                         nsISupportsWeakReference);

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
    if(0 != nsCRT::strcasecmp(aTag, NS_LITERAL_STRING("META").get())) 
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
   return NS_OK;//Notify((nsISupports*)aDocumentID, &keys, &values);
}
NS_IMETHODIMP nsMetaCharsetObserver::Notify(
                     nsISupports* aDocumentID, 
                     const PRUnichar* aTag, 
                     const nsStringArray* keys, const nsStringArray* values)
{
    if(0 != nsCRT::strcasecmp(aTag, NS_LITERAL_STRING("META").get())) 
        return NS_ERROR_ILLEGAL_VALUE;
    else
        return Notify(aDocumentID, keys, values);
}

#define IS_SPACE_CHARS(ch)  (ch == ' ' || ch == '\b' || ch == '\r' || ch == '\n')

NS_IMETHODIMP nsMetaCharsetObserver::Notify(
                    nsISupports* aDocumentID, 
                    const nsStringArray* keys, const nsStringArray* values)
{
    NS_PRECONDITION(keys!=nsnull && values!=nsnull,"Need key-value pair");

    PRInt32 numOfAttributes = keys->Count();
    NS_ASSERTION( numOfAttributes == values->Count(), "size mismatch");
    nsresult res=NS_OK;
#ifdef DEBUG

    PRUnichar Uxcommand[]={'X','_','C','O','M','M','A','N','D','\0'};
    PRUnichar UcharsetSource[]={'c','h','a','r','s','e','t','S','o','u','r','c','e','\0'};
    PRUnichar Ucharset[]={'c','h','a','r','s','e','t','\0'};
    
    NS_ASSERTION(numOfAttributes >= 3, "should have at least 3 private attribute");
    NS_ASSERTION(0==nsCRT::strcmp(Uxcommand,(keys->StringAt(numOfAttributes-1))->get()),"last name should be 'X_COMMAND'" );
    NS_ASSERTION(0==nsCRT::strcmp(UcharsetSource,(keys->StringAt(numOfAttributes-2))->get()),"2nd last name should be 'charsetSource'" );
    NS_ASSERTION(0==nsCRT::strcmp(Ucharset,(keys->StringAt(numOfAttributes-3))->get()),"3rd last name should be 'charset'" );

#endif
    NS_ASSERTION(mAlias, "Didn't get nsICharsetAlias in constructor");

    if(nsnull == mAlias)
      return NS_ERROR_ABORT;

    // we need at least 5 - HTTP-EQUIV, CONTENT and 3 private
    if(numOfAttributes >= 5 ) 
    {
      const PRUnichar *charset = (values->StringAt(numOfAttributes-3))->get();
      const PRUnichar *source =  (values->StringAt(numOfAttributes-2))->get();
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
      const PRUnichar *charsetValue=nsnull;

      for(i=0;i<(numOfAttributes-3);i++)
      {
        const PRUnichar *keyStr;
        keyStr = (keys->StringAt(i))->get();

        //Change 3.190 in nsHTMLTokens.cpp allow  ws/tab/cr/lf exist before 
        // and after text value, this need to be skipped before comparison
        while(IS_SPACE_CHARS(*keyStr)) 
          keyStr++;

        if(0 == nsCRT::strncasecmp(keyStr, NS_LITERAL_STRING("HTTP-EQUIV").get(), 10))
              httpEquivValue=((values->StringAt(i))->get());
        else if(0 == nsCRT::strncasecmp(keyStr, NS_LITERAL_STRING("content").get(), 7))
              contentValue=(values->StringAt(i))->get();
        else if (0 == nsCRT::strncasecmp(keyStr, NS_LITERAL_STRING("charset").get(), 7))
              charsetValue =(values->StringAt(i))->get();
      }
      NS_NAMED_LITERAL_STRING(contenttype, "Content-Type");
      NS_NAMED_LITERAL_STRING(texthtml, "text/html");

      if(nsnull == httpEquivValue || nsnull == contentValue)
        return NS_OK;

      while(IS_SPACE_CHARS(*httpEquivValue)) 
        httpEquivValue++;
      while(IS_SPACE_CHARS(*contentValue)) 
        contentValue++;

      if(
         ((0==nsCRT::strcasecmp(httpEquivValue,contenttype.get())) ||
          (( (httpEquivValue[0]=='\'') ||
             (httpEquivValue[0]=='\"') ) && 
           (0==nsCRT::strncasecmp(httpEquivValue+1,
                      contenttype.get(),
                      contenttype.Length()))
          )) &&
         ((0==nsCRT::strncasecmp(contentValue,texthtml.get(),texthtml.Length())) ||
          (((contentValue[0]=='\'') ||
            (contentValue[0]=='\"'))&&
           (0==nsCRT::strncasecmp(contentValue+1,
                      texthtml.get(),
                      texthtml.Length()))
          ))
        )
      {

         nsAutoString newCharset;

         if (nsnull == charsetValue) 
         {
			 nsAutoString contentPart1(contentValue+10); // after "text/html;"
			 PRInt32 start = contentPart1.RFind("charset=", PR_TRUE ) ;
			 if(kNotFound != start) 
	         {	
				 start += 8; // 8 = "charset=".length 
				 PRInt32 end = contentPart1.FindCharInSet("\'\";", start  );
				 if(kNotFound == end ) 
					 end = contentPart1.Length();
				 NS_ASSERTION(end>=start, "wrong index");
				 contentPart1.Mid(newCharset, start, end - start);
              } 
         }
         else   
         {
             newCharset = charsetValue;
         } 
       
             
         if (!newCharset.IsEmpty())
         {    
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
                        // following charset should have been detected by parser
                        if (!preferred.Equals(NS_LITERAL_STRING("UTF-16")) &&
                            !preferred.Equals(NS_LITERAL_STRING("UTF-16BE")) &&
                            !preferred.Equals(NS_LITERAL_STRING("UTF-16LE")) &&
                            !preferred.Equals(NS_LITERAL_STRING("UTF-32BE")) &&
                            !preferred.Equals(NS_LITERAL_STRING("UTF-32LE"))) {
                          // Propagate the error message so that the parser can
                          // shutdown correctly. - Ref. Bug 96440
                          res = NotifyWebShell(aDocumentID,
                                               NS_ConvertUCS2toUTF8(preferred).get(),
                                               kCharsetFromMetaTag);
                        }
                     } // if(NS_SUCCEEDED(res)
                 }
             } // if EqualIgnoreCase 
         } // if !newCharset.IsEmpty()
      } // if
    }
    else
    {
      nsAutoString compatCharset;
      if (NS_SUCCEEDED(GetCharsetFromCompatibilityTag(keys, values, compatCharset)))
      {
        if (!compatCharset.IsEmpty()) {
          res = NotifyWebShell(aDocumentID, 
                               NS_ConvertUCS2toUTF8(compatCharset).get(), 
                               kCharsetFromMetaTag);
        }
      }
    }
    return res;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::GetCharsetFromCompatibilityTag(
                     const nsStringArray* keys, 
                     const nsStringArray* values, 
                     nsAWritableString& aCharset)
{
    if (!mAlias)
        return NS_ERROR_ABORT;

    aCharset.Truncate(0);
    nsresult res = NS_OK;


    // support for non standard case for compatibility
    // e.g. <META charset="ISO-8859-1">
    PRInt32 numOfAttributes = keys->Count();
    if ((numOfAttributes >= 3) &&
        (0 == nsCRT::strcasecmp((keys->StringAt(0))->get(), NS_LITERAL_STRING("charset").get())))
    {
      nsAutoString srcStr((values->StringAt(numOfAttributes-2))->get());
      PRInt32 err;
      nsCharsetSource  src = (nsCharsetSource) srcStr.ToInteger(&err);
      // if we cannot convert the string into nsCharsetSource, return error
      if (NS_FAILED(err))
          return NS_ERROR_ILLEGAL_VALUE;
      
      // current charset have a lower priority
      if (kCharsetFromMetaTag > src)
      {
          // need nsString for GetPreferred
          nsAutoString newCharset((values->StringAt(0))->get());
          nsAutoString preferred;
          res = mAlias->GetPreferred(newCharset, preferred);
          if (NS_SUCCEEDED(res))
          {
              // compare against the current charset, 
              // also some charsets which should have been found in the BOM detection.
              if (!preferred.Equals((values->StringAt(numOfAttributes-3))->get()) &&
                  !preferred.Equals(NS_LITERAL_STRING("UTF-16")) &&
                  !preferred.Equals(NS_LITERAL_STRING("UTF-16BE")) &&
                  !preferred.Equals(NS_LITERAL_STRING("UTF-16LE")) &&
                  !preferred.Equals(NS_LITERAL_STRING("UTF-32BE")) &&
                  !preferred.Equals(NS_LITERAL_STRING("UTF-32LE")))
                  aCharset = preferred;
          }
      }
    }

  return res;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Observe(nsISupports *aSubject,
                            const PRUnichar *aTopic,
                               const PRUnichar *aData) 
{
  nsresult rv = NS_OK;
  nsDependentString aTopicString(aTopic);
    /*
      Warning!  |NS_LITERAL_STRING(APPSTARTUP_CATEGORY)| relies on the order of
        evaluation of the two |#defines|.  This is known _not_ to work on platforms
        that support wide characters directly.  Better to define |APPSTARTUP_CATEGORY|
        in terms of |NS_LITERAL_STRING| directly.
     */
  if (aTopicString.Equals(NS_LITERAL_STRING(APPSTARTUP_CATEGORY))) //"app_startup"
    rv = Start();
  return rv;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Start() 
{
    nsresult res = NS_OK;

  if (bMetaCharsetObserverStarted == PR_FALSE)  {
    bMetaCharsetObserverStarted = PR_TRUE;

    // get the observer service
    nsCOMPtr<nsIObserverService> anObserverService = 
             do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &res);
    if (NS_FAILED(res)) return res;

    nsAutoString htmlTopic; htmlTopic.AssignWithConversion(kHTMLTextContentType);
    
    // add self to ObserverService
    res = anObserverService->AddObserver(this, htmlTopic.get());
  }

  return res;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::End() 
{
    nsresult res = NS_OK;
  if (bMetaCharsetObserverStarted == PR_TRUE)  {
    bMetaCharsetObserverStarted = PR_FALSE;

    nsCOMPtr<nsIObserverService> anObserverService = 
             do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &res);
    if (NS_FAILED(res)) return res;
     
    nsAutoString htmlTopic; htmlTopic.AssignWithConversion(kHTMLTextContentType);
    res = anObserverService->RemoveObserver(this, htmlTopic.get());
  }
  return res;
}
//========================================================================== 




