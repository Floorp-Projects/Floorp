/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsMediaDocument.h"
#include "nsHTMLAtoms.h"
#include "nsRect.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIScrollable.h"
#include "nsIViewManager.h"
#include "nsITextToSubURI.h"
#include "nsIURL.h"
#include "nsPrintfCString.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDocShell.h"
#include "nsIParser.h" // kCharsetFrom* macro definition
#include "nsIDocumentCharsetInfo.h" 
#include "nsNodeInfoManager.h"

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

nsresult
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

  // We try to set the charset of the current document to that of the 
  // 'genuine' (as opposed to an intervening 'chrome') parent document 
  // that may be in a different window/tab. Even if we fail here,
  // we just return NS_OK because another attempt is made in 
  // |UpdateTitleAndCharset| and the worst thing possible is a mangled 
  // filename in the titlebar and the file picker.

  // When this document is opened in the window/tab of the referring 
  // document (by a simple link-clicking), |prevDocCharacterSet| contains 
  // the charset of the referring document. On the other hand, if the
  // document is opened in a new window, it is |defaultCharacterSet| of |muCV| 
  // where the charset of our interest is stored. In case of openining 
  // in a new tab, we get the charset from |documentCharsetInfo|. Note that we 
  // exclude UTF-8 as 'invalid' because UTF-8 is likely to be the charset 
  // of a chrome document that has nothing to do with the actual content 
  // whose charset we want to know. Even if "the actual content" is indeed 
  // in UTF-8, we don't lose anything because the default empty value is 
  // considered synonymous with UTF-8. 
    
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));

  // not being able to set the charset is not critical.
  NS_ENSURE_TRUE(docShell, NS_OK); 

  nsCOMPtr<nsIDocumentCharsetInfo> dcInfo;
  nsCAutoString charset;

  docShell->GetDocumentCharsetInfo(getter_AddRefs(dcInfo));
  if (dcInfo) {
    nsCOMPtr<nsIAtom> csAtom;
    dcInfo->GetParentCharset(getter_AddRefs(csAtom));
    if (csAtom) {   // opening in a new tab
      csAtom->ToUTF8String(charset);
    }
  }

  if (charset.IsEmpty() || charset.Equals("UTF-8")) {
    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));

    // not being able to set the charset is not critical.
    NS_ENSURE_TRUE(cv, NS_OK); 
    nsCOMPtr<nsIMarkupDocumentViewer> muCV = do_QueryInterface(cv);
    if (muCV) {
      muCV->GetPrevDocCharacterSet(charset);   // opening in the same window/tab
      if (charset.Equals("UTF-8") || charset.IsEmpty()) {
        muCV->GetDefaultCharacterSet(charset); // opening in a new window
      }
    } 
  }

  if (!charset.IsEmpty() && !charset.Equals("UTF-8")) {
    SetDocumentCharacterSet(charset);
    mCharacterSetSource = kCharsetFromUserDefault;
  }

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

  nsCOMPtr<nsIHTMLContent> root = NS_NewHTMLHtmlElement(nodeInfo);
  if (!root) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  root->SetDocument(this, PR_FALSE, PR_TRUE);
  SetRootContent(root);

  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::body, nsnull,
                                     kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLContent> body = NS_NewHTMLBodyElement(nodeInfo);
  if (!body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  body->SetDocument(this, PR_FALSE, PR_TRUE);
  mBodyContent = do_QueryInterface(body);

  root->AppendChildTo(body, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsresult
nsMediaDocument::StartLayout()
{
  PRUint32 numberOfShells = GetNumberOfShells();
  for (PRUint32 i = 0; i < numberOfShells; i++) {
    nsIPresShell *shell = GetShellAt(i);

    // Make shell an observer for next time.
    shell->BeginObservingDocument();

    // Initial-reflow this time.
    nsRect visibleArea = shell->GetPresContext()->GetVisibleArea();
    shell->InitialReflow(visibleArea.width, visibleArea.height);

    // Now trigger a refresh.
    nsIViewManager* vm = shell->GetViewManager();
    if (vm) {
      vm->EnableRefresh(NS_VMREFRESH_IMMEDIATE);
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
  if (mDocumentURI) {
    nsCAutoString fileName;
    nsCOMPtr<nsIURL> url = do_QueryInterface(mDocumentURI);
    if (url)
      url->GetFileName(fileName);

    nsCAutoString docCharset;

    // Now that the charset is set in |StartDocumentLoad| to the charset of
    // the document viewer instead of a bogus value ("ISO-8859-1" set in
    // |nsDocument|'s ctor), the priority is given to the current charset. 
    // This is necessary to deal with a media document being opened in a new 
    // window or a new tab, in which case |originCharset| of |nsIURI| is not 
    // reliable.
    if (mCharacterSetSource != kCharsetUninitialized) {  
      docCharset = mCharacterSet;
    }
    else {  
      // resort to |originCharset|
      mDocumentURI->GetOriginCharset(docCharset);
      SetDocumentCharacterSet(docCharset);
    }
    if (!fileName.IsEmpty()) {
      nsresult rv;
      nsCOMPtr<nsITextToSubURI> textToSubURI = 
        do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv))
        rv = textToSubURI->UnEscapeURIForUI(docCharset, fileName, fileStr);
    }
    if (fileStr.IsEmpty())
      CopyUTF8toUTF16(fileName, fileStr);
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
