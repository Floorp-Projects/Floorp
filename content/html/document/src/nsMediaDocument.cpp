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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jungshik Shin <jshin@mailaps.org>
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

#include "nsMediaDocument.h"
#include "nsHTMLAtoms.h"
#include "nsRect.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIScrollable.h"
#include "nsIViewManager.h"
#include "nsITextToSubURI.h"
#include "nsIURL.h"
#include "nsPrintfCString.h"

nsMediaDocumentStreamListener::nsMediaDocumentStreamListener(nsMediaDocument *aDocument)
{
  mDocument = aDocument;
}

nsMediaDocumentStreamListener::~nsMediaDocumentStreamListener()
{
}


NS_IMPL_THREADSAFE_ISUPPORTS2(nsMediaDocumentStreamListener,
                              nsIRequestObserver,
                              nsIStreamListener)


void
nsMediaDocumentStreamListener::SetStreamListener(nsIStreamListener *aListener)
{
  mNextStream = aListener;
}

NS_IMETHODIMP
nsMediaDocumentStreamListener::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  mDocument->StartLayout();

  if (mNextStream) {
    return mNextStream->OnStartRequest(request, ctxt);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMediaDocumentStreamListener::OnStopRequest(nsIRequest* request,
                                             nsISupports *ctxt,
                                             nsresult status)
{
  nsresult rv = NS_OK;
  if (mNextStream) {
    rv = mNextStream->OnStopRequest(request, ctxt, status);
  }

  // No more need for our document so clear our reference and prevent leaks
  mDocument = nsnull;

  return rv;
}

NS_IMETHODIMP
nsMediaDocumentStreamListener::OnDataAvailable(nsIRequest* request,
                                               nsISupports *ctxt,
                                               nsIInputStream *inStr,
                                               PRUint32 sourceOffset,
                                               PRUint32 count)
{
  if (mNextStream) {
    return mNextStream->OnDataAvailable(request, ctxt, inStr, sourceOffset, count);
  }

  return NS_OK;
}

// default format names for nsMediaDocument. 
const char* const nsMediaDocument::sFormatNames[4] = 
{
  "MediaTitleWithNoInfo",    // eWithNoInfo
  "MediaTitleWithFile",      // eWithFile
  "",                        // eWithDim
  ""                         // eWithDimAndFile
};

nsMediaDocument::nsMediaDocument()
{
}
nsMediaDocument::~nsMediaDocument()
{
}

nsresult
nsMediaDocument::Init()
{
  nsresult rv = nsHTMLDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a bundle for the localization
  nsCOMPtr<nsIStringBundleService> stringService(
    do_GetService(NS_STRINGBUNDLE_CONTRACTID));
  if (stringService) {
    stringService->CreateBundle(NSMEDIADOCUMENT_PROPERTIES_URI,
                                getter_AddRefs(mStringBundle));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMediaDocument::StartDocumentLoad(const char*         aCommand,
                                   nsIChannel*         aChannel,
                                   nsILoadGroup*       aLoadGroup,
                                   nsISupports*        aContainer,
                                   nsIStreamListener** aDocListener,
                                   PRBool              aReset,
                                   nsIContentSink*     aSink)
{
  nsresult rv = nsDocument::StartDocumentLoad(aCommand, aChannel, aLoadGroup,
                                              aContainer, aDocListener, aReset,
                                              aSink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RetrieveRelevantHeaders(aChannel);

  return NS_OK;
}

nsresult
nsMediaDocument::CreateSyntheticDocument()
{
  // Synthesize an empty html document
  nsresult rv;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::html, nsnull,
                                     kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLContent> root;
  rv = NS_NewHTMLHtmlElement(getter_AddRefs(root), nodeInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }
  root->SetDocument(this, PR_FALSE, PR_TRUE);
  SetRootContent(root);

  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::body, nsnull,
                                     kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLContent> body;
  rv = NS_NewHTMLBodyElement(getter_AddRefs(body), nodeInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }
  body->SetDocument(this, PR_FALSE, PR_TRUE);
  mBodyContent = do_QueryInterface(body);

  root->AppendChildTo(body, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsresult
nsMediaDocument::StartLayout()
{
  // Reset scrolling to default settings for this shell.
  // This must happen before the initial reflow, when we create the root frame
  nsCOMPtr<nsIScrollable> scrollableContainer(do_QueryReferent(mDocumentContainer));
  if (scrollableContainer) {
    scrollableContainer->ResetScrollbarPreferences();
  }

  PRInt32 numberOfShells = GetNumberOfShells();
  for (PRInt32 i = 0; i < numberOfShells; i++) {
    nsCOMPtr<nsIPresShell> shell;
    GetShellAt(i, getter_AddRefs(shell));
    if (shell) {
      // Make shell an observer for next time.
      shell->BeginObservingDocument();

      // Initial-reflow this time.
      nsCOMPtr<nsIPresContext> context;
      shell->GetPresContext(getter_AddRefs(context));
      nsRect visibleArea;
      context->GetVisibleArea(visibleArea);
      shell->InitialReflow(visibleArea.width, visibleArea.height);

      // Now trigger a refresh.
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
      }
    }
  }

  return NS_OK;
}

void 
nsMediaDocument::UpdateTitleAndCharset(const nsACString& aTypeStr,
                                       const char* const* aFormatNames,
                                       PRInt32 aWidth, PRInt32 aHeight,
                                       const nsAString& aStatus)
{
  nsXPIDLString fileStr;
  nsCOMPtr<nsIURI> uri = do_QueryInterface(mDocumentURL);
  if (uri) {
    nsCAutoString originCharset;
    uri->GetOriginCharset(originCharset);
    nsCAutoString fileName;
    nsCOMPtr<nsIURL> url = do_QueryInterface(uri);
    if (url)
      url->GetFileName(fileName);
    if (!originCharset.IsEmpty()) {
      // set doc. charset to that of the referring document if known so that
      // filepicker comes up with the correct non-ascii filename.
      SetDocumentCharacterSet(originCharset);
      if (!fileName.IsEmpty()) {
        nsresult rv;
        nsCOMPtr<nsITextToSubURI> textToSubURI = 
          do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
          rv = textToSubURI->UnEscapeURIForUI(originCharset, fileName, fileStr);
      }
    }
    if (fileStr.IsEmpty())
      fileStr.Assign(NS_ConvertUTF8toUCS2(fileName));
  }


  NS_ConvertASCIItoUCS2 typeStr(aTypeStr);
  nsXPIDLString title;

  if (mStringBundle) {
    // if we got a valid size (not all media have a size)
    if (aWidth != 0 && aHeight != 0) {
      nsAutoString widthStr;
      nsAutoString heightStr;
      widthStr.AppendInt(aWidth);
      heightStr.AppendInt(aHeight);
      // If we got a filename, display it
      if (!fileStr.IsEmpty()) {
        const PRUnichar *formatStrings[4]  = {fileStr.get(), typeStr.get(), 
          widthStr.get(), heightStr.get()};
        NS_ConvertASCIItoUCS2 fmtName(aFormatNames[eWithDimAndFile]);
        mStringBundle->FormatStringFromName(fmtName.get(), formatStrings, 4,
                                            getter_Copies(title));
      } 
      else {
        const PRUnichar *formatStrings[3]  = {typeStr.get(), widthStr.get(), 
          heightStr.get()};
        NS_ConvertASCIItoUCS2 fmtName(aFormatNames[eWithDim]);
        mStringBundle->FormatStringFromName(fmtName.get(), formatStrings, 3,
                                            getter_Copies(title));
      }
    } 
    else {
    // If we got a filename, display it
      if (!fileStr.IsEmpty()) {
        const PRUnichar *formatStrings[2] = {fileStr.get(), typeStr.get()};
        NS_ConvertASCIItoUCS2 fmtName(aFormatNames[eWithFile]);
        mStringBundle->FormatStringFromName(fmtName.get(), formatStrings, 2,
                                            getter_Copies(title));
      }
      else {
        const PRUnichar *formatStrings[1] = {typeStr.get()};
        NS_ConvertASCIItoUCS2 fmtName(aFormatNames[eWithNoInfo]);
        mStringBundle->FormatStringFromName(fmtName.get(), formatStrings, 1,
                                            getter_Copies(title));
      }
    }
  } 

  // set it on the document
  if (aStatus.IsEmpty()) {
    SetTitle(title);
  }
  else {
    nsXPIDLString titleWithStatus;
    const nsPromiseFlatString& status = PromiseFlatString(aStatus);
    const PRUnichar *formatStrings[2] = {title.get(), status.get()};
    NS_NAMED_LITERAL_STRING(fmtName, "TitleWithStatus");
    mStringBundle->FormatStringFromName(fmtName.get(), formatStrings, 2,
                                        getter_Copies(titleWithStatus));
    SetTitle(titleWithStatus);
  }
}
