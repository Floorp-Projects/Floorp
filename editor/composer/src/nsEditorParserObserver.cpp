/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   steve clark <buster@netscape.com>
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

#include "nsIServiceManager.h"

#include "nsEditorParserObserver.h"


NS_IMPL_ADDREF(nsEditorParserObserver);
NS_IMPL_RELEASE(nsEditorParserObserver);

NS_INTERFACE_MAP_BEGIN(nsEditorParserObserver)
      NS_INTERFACE_MAP_ENTRY(nsIElementObserver)
      NS_INTERFACE_MAP_ENTRY(nsIObserver)
      NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
      NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIElementObserver)
NS_INTERFACE_MAP_END

nsEditorParserObserver::nsEditorParserObserver()
: mBadTagFound(PR_FALSE)
{
  NS_INIT_REFCNT();
}

nsEditorParserObserver::~nsEditorParserObserver()
{
}

NS_IMETHODIMP_(const char*) nsEditorParserObserver::GetTagNameAt(PRUint32 aTagIndex)
{
  nsCString* theString = mWatchTags[aTagIndex];
  if (theString)
    return theString->get();
  else
    return nsnull;
}

NS_IMETHODIMP nsEditorParserObserver::Notify(
                     PRUint32 aDocumentID, 
                     const PRUnichar* aTag, 
                     PRUint32 numOfAttributes, 
                     const PRUnichar* nameArray[], 
                     const PRUnichar* valueArray[])
{
  // XXX based on aTag...
  Notify();
  return NS_OK;
}

NS_IMETHODIMP nsEditorParserObserver::Notify(
                     PRUint32 aDocumentID, 
                     eHTMLTags aTag, 
                     PRUint32 numOfAttributes, 
                     const PRUnichar* nameArray[], 
                     const PRUnichar* valueArray[])
{
  if (eHTMLTag_frameset == aTag)
  {
    Notify();
    return NS_OK;
  }
  else
    return NS_ERROR_ILLEGAL_VALUE;
}
NS_IMETHODIMP nsEditorParserObserver::Notify(nsISupports* aDocumentID, const PRUnichar* aTag, 
                                             const nsStringArray* aKeys, const nsStringArray* aValues)
{
  Notify();
  return NS_OK;
}

NS_IMETHODIMP nsEditorParserObserver::Observe(nsISupports*, const PRUnichar*, const PRUnichar*)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void nsEditorParserObserver::Notify()
{
  mBadTagFound = PR_TRUE;
}

NS_IMETHODIMP nsEditorParserObserver::Start() 
{
  nsresult res = NS_OK;
  nsAutoString parserService; parserService.AssignWithConversion("text/html");

  nsCOMPtr<nsIObserverService> anObserverService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &res);
  if (NS_FAILED(res)) return res;
   
  return anObserverService->AddObserver(this, parserService.get());
}

NS_IMETHODIMP nsEditorParserObserver::End() 
{
  nsresult res = NS_OK;
  nsAutoString parserService; parserService.AssignWithConversion("text/html");

  nsCOMPtr<nsIObserverService> anObserverService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &res);
  if (NS_FAILED(res)) return res;
   
  return anObserverService->RemoveObserver(this, parserService.get());
}

NS_IMETHODIMP nsEditorParserObserver::GetBadTagFound(PRBool *aFound)
{
  NS_ENSURE_ARG_POINTER(aFound);
  *aFound = mBadTagFound;
  return NS_OK; 
}


NS_IMETHODIMP nsEditorParserObserver::RegisterTagToWatch(const char* tagName)
{
  nsCString theTagString(tagName);
  mWatchTags.AppendCString(theTagString);
  return NS_OK;
}



