/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "pratom.h"
#include "nsCharDetDll.h"
#include "nsIServiceManager.h"
#include "nsObserverBase.h"
#include "nsWeakReference.h"
#include "nsIParserService.h"
#include "nsParserCIID.h"
#include "nsMetaCharsetCID.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

static NS_DEFINE_CID(kCharsetAliasCID, NS_CHARSETALIAS_CID);
static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);
 
static const eHTMLTags gWatchTags[] = 
{ eHTMLTag_meta,
  eHTMLTag_unknown
};

//-------------------------------------------------------------------------
nsMetaCharsetObserver::nsMetaCharsetObserver()
{
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
}

//-------------------------------------------------------------------------
NS_IMPL_ADDREF ( nsMetaCharsetObserver )
NS_IMPL_RELEASE ( nsMetaCharsetObserver )

// Use the new scheme
NS_IMPL_QUERY_INTERFACE4(nsMetaCharsetObserver, 
                         nsIElementObserver, 
                         nsIObserver, 
                         nsIMetaCharsetService, 
                         nsISupportsWeakReference)

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Notify(
                     PRUint32 aDocumentID, 
                     const PRUnichar* aTag, 
                     PRUint32 numOfAttributes, 
                     const PRUnichar* nameArray[], 
                     const PRUnichar* valueArray[])
{
  
    if(!nsDependentString(aTag).LowerCaseEqualsLiteral("meta")) 
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
                     nsISupports* aWebShell,
                     nsISupports* aChannel,
                     const PRUnichar* aTag, 
                     const nsStringArray* keys, 
                     const nsStringArray* values,
                     const PRUint32 aFlags)
{
  nsresult result = NS_OK;
  // bug 125317 - document.write content is already an unicode content.
  if (!(aFlags & nsIElementObserver::IS_DOCUMENT_WRITE)) {
    if(!nsDependentString(aTag).LowerCaseEqualsLiteral("meta")) {
        result = NS_ERROR_ILLEGAL_VALUE;
    }
    else {
        result = Notify(aWebShell, aChannel, keys, values);
    }
  }
  return result;
}

#define IS_SPACE_CHARS(ch)  (ch == ' ' || ch == '\b' || ch == '\r' || ch == '\n')

NS_IMETHODIMP nsMetaCharsetObserver::Notify(
                    nsISupports* aWebShell,
                    nsISupports* aChannel,
                    const nsStringArray* keys, 
                    const nsStringArray* values)
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
      PRInt32  src = srcStr.ToInteger(&err);
      // if we cannot convert the string into PRInt32, return error
      NS_ASSERTION(NS_SUCCEEDED(err), "cannot get charset source");
      if(NS_FAILED(err))
          return NS_ERROR_ILLEGAL_VALUE;

      if(kCharsetFromMetaTag <= src)
          return NS_OK; // current charset has higher priority. don't bother to do the following

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

        if(Substring(keyStr, keyStr+10).LowerCaseEqualsLiteral("http-equiv"))
              httpEquivValue = values->StringAt(i)->get();
        else if(Substring(keyStr, keyStr+7).LowerCaseEqualsLiteral("content"))
              contentValue = values->StringAt(i)->get();
        else if (Substring(keyStr, keyStr+7).LowerCaseEqualsLiteral("charset"))
              charsetValue = values->StringAt(i)->get();
      }
      NS_NAMED_LITERAL_STRING(contenttype, "Content-Type");
      NS_NAMED_LITERAL_STRING(texthtml, "text/html");

      if(nsnull == httpEquivValue || nsnull == contentValue)
        return NS_OK;

      while(IS_SPACE_CHARS(*httpEquivValue))
        ++httpEquivValue;
      // skip opening quote
      if (*httpEquivValue == '\'' || *httpEquivValue == '\"')
        ++httpEquivValue;

      while(IS_SPACE_CHARS(*contentValue))
        ++contentValue;
      // skip opening quote
      if (*contentValue == '\'' || *contentValue == '\"')
        ++contentValue;

      if(
         Substring(httpEquivValue,
                   httpEquivValue+contenttype.Length()).Equals(contenttype,
                                                               nsCaseInsensitiveStringComparator())
         &&
         Substring(contentValue,
                   contentValue+texthtml.Length()).Equals(texthtml,
                                                          nsCaseInsensitiveStringComparator())
        )
      {

         nsCAutoString newCharset;

         if (nsnull == charsetValue) 
         {
           nsAutoString contentPart1(contentValue+9); // after "text/html"
           PRInt32 start = contentPart1.RFind("charset=", PR_TRUE ) ;
           PRInt32 end = contentPart1.Length();
           if(kNotFound != start)
           {
             start += 8; // 8 = "charset=".length 
             while (start < end && contentPart1.CharAt(start) == PRUnichar(' '))
               ++start;
             if (start < end) {
               end = contentPart1.FindCharInSet("\'\"; ", start);
               if(kNotFound == end ) 
                 end = contentPart1.Length();
               NS_ASSERTION(end>=start, "wrong index");
               LossyCopyUTF16toASCII(Substring(contentPart1, start, end-start),
                                     newCharset);
             }
           } 
         }
         else   
         {
             LossyCopyUTF16toASCII(nsDependentString(charsetValue), newCharset);
         } 

         nsCAutoString charsetString; charsetString.AssignWithConversion(charset);
         
         if (!newCharset.IsEmpty())
         {    
             if(! newCharset.Equals(charsetString, nsCaseInsensitiveCStringComparator()))
             {
                 PRBool same = PR_FALSE;
                 nsresult res2 = mAlias->Equals( newCharset, charsetString , &same);
                 if(NS_SUCCEEDED(res2) && (! same))
                 {
                     nsCAutoString preferred;
                     res2 = mAlias->GetPreferred(newCharset, preferred);
                     if(NS_SUCCEEDED(res2))
                     {
                        // following charset should have been detected by parser
                        if (!preferred.EqualsLiteral("UTF-16") &&
                            !preferred.EqualsLiteral("UTF-16BE") &&
                            !preferred.EqualsLiteral("UTF-16LE") &&
                            !preferred.EqualsLiteral("UTF-32BE") &&
                            !preferred.EqualsLiteral("UTF-32LE")) {
                          // Propagate the error message so that the parser can
                          // shutdown correctly. - Ref. Bug 96440
                          res = NotifyWebShell(aWebShell,
                                               aChannel,
                                               preferred.get(),
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
          res = NotifyWebShell(aWebShell,
                               aChannel,
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
                     nsAString& aCharset)
{
    if (!mAlias)
        return NS_ERROR_ABORT;

    aCharset.Truncate(0);
    nsresult res = NS_OK;


    // support for non standard case for compatibility
    // e.g. <META charset="ISO-8859-1">
    PRInt32 numOfAttributes = keys->Count();
    if ((numOfAttributes >= 3) &&
        (keys->StringAt(0)->LowerCaseEqualsLiteral("charset")))
    {
      nsAutoString srcStr((values->StringAt(numOfAttributes-2))->get());
      PRInt32 err;
      PRInt32  src = srcStr.ToInteger(&err);
      // if we cannot convert the string into PRInt32, return error
      if (NS_FAILED(err))
          return NS_ERROR_ILLEGAL_VALUE;
      
      // current charset have a lower priority
      if (kCharsetFromMetaTag > src)
      {
          nsCAutoString newCharset;
          newCharset.AssignWithConversion(values->StringAt(0)->get());
          
          nsCAutoString preferred;
          res = mAlias->GetPreferred(newCharset,
                                     preferred);
          if (NS_SUCCEEDED(res))
          {
              // compare against the current charset, 
              // also some charsets which should have been found in
              // the BOM detection.
              nsString* currentCharset = values->StringAt(numOfAttributes-3);
              if (!preferred.Equals(NS_LossyConvertUCS2toASCII(*currentCharset)) &&
                  !preferred.EqualsLiteral("UTF-16") &&
                  !preferred.EqualsLiteral("UTF-16BE") &&
                  !preferred.EqualsLiteral("UTF-16LE") &&
                  !preferred.EqualsLiteral("UTF-32BE") &&
                  !preferred.EqualsLiteral("UTF-32LE"))
                  AppendASCIItoUTF16(preferred, aCharset);
          }
      }
    }

  return res;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Observe(nsISupports *aSubject,
                            const char *aTopic,
                               const PRUnichar *aData) 
{
  nsresult rv = NS_OK;
  if (!nsCRT::strcmp(aTopic, "parser-service-start")) {
    rv = Start();
  }
  return rv;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::Start() 
{
  nsresult rv = NS_OK;

  if (bMetaCharsetObserverStarted == PR_FALSE)  {
    bMetaCharsetObserverStarted = PR_TRUE;

    nsCOMPtr<nsIParserService> parserService(do_GetService(kParserServiceCID, &rv));

    if (NS_FAILED(rv))
      return rv;

    rv = parserService->RegisterObserver(this,
                                         NS_LITERAL_STRING("text/html"),
                                         gWatchTags);
  }

  return rv;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMetaCharsetObserver::End() 
{
  nsresult rv = NS_OK;
  if (bMetaCharsetObserverStarted == PR_TRUE)  {
    bMetaCharsetObserverStarted = PR_FALSE;

    nsCOMPtr<nsIParserService> parserService(do_GetService(kParserServiceCID, &rv));

    if (NS_FAILED(rv))
      return rv;
    
    rv = parserService->UnregisterObserver(this, NS_LITERAL_STRING("text/html"));
  }
  return rv;
}
//========================================================================== 




