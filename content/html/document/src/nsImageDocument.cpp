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
 *   Morten Nilsen <morten@nilsen.com>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Jan Varga <varga@netscape.com>
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

#include "nsRect.h"
#include "nsHTMLDocument.h"
#include "nsIImageDocument.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventListener.h"
#include "nsHTMLAtoms.h"
#include "imgIRequest.h"
#include "imgILoader.h"
#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "nsIURL.h"
#include "nsIScrollable.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsIViewManager.h"
#include "nsIStringBundle.h"
#include "nsIPrefService.h"

#define NSIMAGEDOCUMENT_PROPERTIES_URI "chrome://communicator/locale/layout/ImageDocument.properties"
#define AUTOMATIC_IMAGE_RESIZING_PREF "browser.enable_automatic_image_resizing"


class nsImageDocument : public nsHTMLDocument,
                        public nsIImageDocument,
                        public nsIStreamListener,
                        public imgIDecoderObserver,
                        public nsIDOMEventListener
{
public:
  nsImageDocument();
  virtual ~nsImageDocument();

  NS_DECL_ISUPPORTS

  // nsIHTMLDocument
  nsresult Init();

  NS_IMETHOD StartDocumentLoad(const char*         aCommand,
                               nsIChannel*         aChannel,
                               nsILoadGroup*       aLoadGroup,
                               nsISupports*        aContainer,
                               nsIStreamListener** aDocListener,
                               PRBool              aReset = PR_TRUE,
                               nsIContentSink*     aSink = nsnull);

  NS_IMETHOD SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject);

  NS_DECL_NSIIMAGEDOCUMENT

  NS_DECL_NSIREQUESTOBSERVER

  NS_DECL_NSISTREAMLISTENER

  NS_DECL_IMGIDECODEROBSERVER

  NS_DECL_IMGICONTAINEROBSERVER

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

protected:
  nsresult CreateSyntheticDocument();

  nsresult CheckOverflowing();

  nsresult StartLayout();

  nsresult UpdateTitle();

  nsCOMPtr<nsIStringBundle>     mStringBundle;
  nsCOMPtr<nsIDOMElement>       mImageElement;
  nsCOMPtr<imgIRequest>         mImageRequest;
  nsCOMPtr<nsIStreamListener>   mNextStream;

  nscoord                       mVisibleWidth;
  nscoord                       mVisibleHeight;
  nscoord                       mImageWidth;
  nscoord                       mImageHeight;

  PRPackedBool                  mImageResizingEnabled;
  PRPackedBool                  mImageIsOverflowing;
  PRPackedBool                  mImageIsResized;
};


nsImageDocument::nsImageDocument()
  : mVisibleWidth(0),
    mVisibleHeight(0),
    mImageWidth(0),
    mImageHeight(0),
    mImageResizingEnabled(PR_FALSE),
    mImageIsOverflowing(PR_FALSE),
    mImageIsResized(PR_FALSE)
{
}

nsImageDocument::~nsImageDocument()
{
}

NS_IMPL_ADDREF_INHERITED(nsImageDocument, nsHTMLDocument)
NS_IMPL_RELEASE_INHERITED(nsImageDocument, nsHTMLDocument)

NS_INTERFACE_MAP_BEGIN(nsImageDocument)
  NS_INTERFACE_MAP_ENTRY(nsIImageDocument)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(imgIDecoderObserver)
  NS_INTERFACE_MAP_ENTRY(imgIContainerObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(ImageDocument)
NS_INTERFACE_MAP_END_INHERITING(nsHTMLDocument)


nsresult
nsImageDocument::Init()
{
  nsresult rv = nsHTMLDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefBranch) {
    PRBool temp = PR_FALSE;
    prefBranch->GetBoolPref(AUTOMATIC_IMAGE_RESIZING_PREF, &temp);
    mImageResizingEnabled = temp;
  }

  // Create a bundle for the localization
  nsCOMPtr<nsIStringBundleService> stringService(
    do_GetService(NS_STRINGBUNDLE_CONTRACTID));
  if (stringService) {
    stringService->CreateBundle(NSIMAGEDOCUMENT_PROPERTIES_URI,
                                getter_AddRefs(mStringBundle));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::StartDocumentLoad(const char*         aCommand,
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

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    // The misspelled key 'referer' is as per the HTTP spec
    nsCAutoString referrerHeader;
    rv = httpChannel->GetRequestHeader(NS_LITERAL_CSTRING("referer"),
                                       referrerHeader);
    
    if (NS_SUCCEEDED(rv)) {
      SetReferrer(NS_ConvertASCIItoUCS2(referrerHeader));
    }
  }

  // Create synthetic document
  rv = CreateSyntheticDocument();
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ASSERTION(aDocListener, "null aDocListener");
  NS_ADDREF(*aDocListener = this);

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aScriptGlobalObject)
{
  if (!aScriptGlobalObject) {
    // If the global object is being set to null, then it means we are
    // going away soon. Drop our ref to imgRequest so that we don't end
    // up leaking due to cycles through imgLib
    mImageRequest = nsnull;

    if (mImageResizingEnabled) {
      nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mImageElement);
      target->RemoveEventListener(NS_LITERAL_STRING("click"), this, PR_FALSE);

      target = do_QueryInterface(mScriptGlobalObject);
      target->RemoveEventListener(NS_LITERAL_STRING("resize"), this, PR_FALSE);
      target->RemoveEventListener(NS_LITERAL_STRING("keypress"), this, PR_FALSE);
    }
  }
  else {
    if (mImageResizingEnabled) {
      nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mImageElement);
      target->AddEventListener(NS_LITERAL_STRING("click"), this, PR_FALSE);

      target = do_QueryInterface(aScriptGlobalObject);
      target->AddEventListener(NS_LITERAL_STRING("resize"), this, PR_FALSE);
      target->AddEventListener(NS_LITERAL_STRING("keypress"), this, PR_FALSE);
    }
  }

  return nsHTMLDocument::SetScriptGlobalObject(aScriptGlobalObject);
}


NS_IMETHODIMP
nsImageDocument::GetImageResizingEnabled(PRBool* aImageResizingEnabled)
{
  *aImageResizingEnabled = mImageResizingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::GetImageIsOverflowing(PRBool* aImageIsOverflowing)
{
  *aImageIsOverflowing = mImageIsOverflowing;
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::GetImageIsResized(PRBool* aImageIsResized)
{
  *aImageIsResized = mImageIsResized;
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::ShrinkToFit()
{
  if (mImageResizingEnabled) {
    nsCOMPtr<nsIDOMHTMLImageElement> image = do_QueryInterface(mImageElement);

    float ratio = PR_MIN((float)mVisibleWidth / mImageWidth,
                         (float)mVisibleHeight / mImageHeight);
    image->SetWidth(NSToCoordFloor(mImageWidth * ratio));

    mImageElement->SetAttribute(NS_LITERAL_STRING("style"),
                                NS_LITERAL_STRING("cursor: move"));

    mImageIsResized = PR_TRUE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::RestoreImage()
{
  if (mImageResizingEnabled) {
    mImageElement->RemoveAttribute(NS_LITERAL_STRING("width"));

    if (!mImageIsOverflowing) {
      mImageElement->RemoveAttribute(NS_LITERAL_STRING("style"));
    }

    mImageIsResized = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::ToggleImageSize()
{
  if (mImageResizingEnabled) {
    if (mImageIsResized) {
      RestoreImage();
    }
    else if (mImageIsOverflowing) {
      ShrinkToFit();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (!channel) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresShell> shell;
  nsCOMPtr<nsIPresContext> context;
  GetShellAt(0, getter_AddRefs(shell));
  if (shell) {
    shell->GetPresContext(getter_AddRefs(context));
  }

  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1"));
  il->LoadImageWithChannel(channel, this, context, getter_AddRefs(mNextStream), 
                           getter_AddRefs(mImageRequest));

  StartLayout();

  if (mNextStream) {
    return mNextStream->OnStartRequest(request, ctxt);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                             nsresult status)
{
  UpdateTitle();

  if (mNextStream) {
    return mNextStream->OnStopRequest(request, ctxt, status);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnDataAvailable(nsIRequest* request, nsISupports *ctxt,
                                 nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  if (mNextStream) {
    return mNextStream->OnDataAvailable(request, ctxt, inStr, sourceOffset, count);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsImageDocument::OnStartDecode(imgIRequest* aRequest,
                               nsISupports* aCX)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStartContainer(imgIRequest* aRequest,
                                  nsISupports* aCX,
                                  imgIContainer* aImage)
{
  aImage->GetWidth(&mImageWidth);
  aImage->GetHeight(&mImageHeight);
  if (mImageResizingEnabled) {
    CheckOverflowing();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStartFrame(imgIRequest* aRequest,
                              nsISupports* aCX,
                              gfxIImageFrame* aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnDataAvailable(imgIRequest* aRequest,
                                 nsISupports *aCX,
                                 gfxIImageFrame* aFrame,
                                 const nsRect* aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStopFrame(imgIRequest* aRequest,
                             nsISupports* aCX,
                             gfxIImageFrame* aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStopContainer(imgIRequest* aRequest,
                                 nsISupports* aCX,
                                 imgIContainer* aImage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::OnStopDecode(imgIRequest* aRequest,
                              nsISupports* aCX,
                              nsresult status,
                              const PRUnichar* statusArg)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageDocument::FrameChanged(imgIContainer* aContainer,
                              nsISupports *aCX,
                              gfxIImageFrame* aFrame,
                              nsRect* aDirtyRect)
{
  return NS_OK;
}


NS_IMETHODIMP
nsImageDocument::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.Equals(NS_LITERAL_STRING("resize"))) {
    CheckOverflowing();
  }
  else if (eventType.Equals(NS_LITERAL_STRING("click"))) {
    ToggleImageSize();
  }
  else if (eventType.Equals(NS_LITERAL_STRING("keypress"))) {
    nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aEvent);
    PRUint32 charCode;
    keyEvent->GetCharCode(&charCode);
    // plus key
    if (charCode == 0x2B) {
      if (mImageIsResized) {
        RestoreImage();
      }
    }
    // minus key
    else if (charCode == 0x2D) {
      if (mImageIsOverflowing) {
        ShrinkToFit();
      }
    }
  }

  return NS_OK;
}

nsresult
nsImageDocument::CreateSyntheticDocument()
{
  // Synthesize an html document that refers to the image
  nsresult rv;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::html, nsnull,
                                     kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
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
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLContent> body;
  rv = NS_NewHTMLBodyElement(getter_AddRefs(body), nodeInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }
  body->SetDocument(this, PR_FALSE, PR_TRUE);
  mBodyContent = do_QueryInterface(body);

  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::img, nsnull,
                                     kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLContent> image;
  rv = NS_NewHTMLImageElement(getter_AddRefs(image), nodeInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }
  image->SetDocument(this, PR_FALSE, PR_TRUE);
  mImageElement = do_QueryInterface(image);

  nsCAutoString src;
  mDocumentURL->GetSpec(src);

  NS_ConvertUTF8toUCS2 srcString(src);
  nsHTMLValue val(srcString);

  image->SetHTMLAttribute(nsHTMLAtoms::src, val, PR_FALSE);

  if (mStringBundle) {
    const PRUnichar* formatString[1] = { srcString.get() };
    nsXPIDLString errorMsg;
    rv = mStringBundle->FormatStringFromName(NS_LITERAL_STRING("InvalidImage").get(),
      formatString, 1, getter_Copies(errorMsg));

    nsHTMLValue errorText(errorMsg);
    image->SetHTMLAttribute(nsHTMLAtoms::alt, errorText, PR_FALSE);
  }

  root->AppendChildTo(body, PR_FALSE, PR_FALSE);
  body->AppendChildTo(image, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsresult
nsImageDocument::CheckOverflowing()
{
  nsCOMPtr<nsIPresShell> shell;
  GetShellAt(0, getter_AddRefs(shell));
  if (!shell) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));

  nsRect visibleArea;
  context->GetVisibleArea(visibleArea);

  nsCOMPtr<nsIStyleContext> styleContext;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mBodyContent);
  context->ResolveStyleContextFor(content, nsnull, getter_AddRefs(styleContext));

  const nsStyleMargin* marginData =
    (const nsStyleMargin*)styleContext->GetStyleData(eStyleStruct_Margin);
  nsMargin margin;
  marginData->GetMargin(margin);
  visibleArea.Deflate(margin);

  nsStyleBorderPadding bPad;
  styleContext->GetBorderPaddingFor(bPad);
  bPad.GetBorderPadding(margin);
  visibleArea.Deflate(margin);

  float t2p;
  context->GetTwipsToPixels(&t2p);
  mVisibleWidth = NSTwipsToIntPixels(visibleArea.width, t2p);
  mVisibleHeight = NSTwipsToIntPixels(visibleArea.height, t2p);

  mImageIsOverflowing = mImageWidth > mVisibleWidth || mImageHeight > mVisibleHeight;

  if (mImageIsOverflowing) {
    ShrinkToFit();
  }
  else if (mImageIsResized) {
    RestoreImage();
  }

  return NS_OK;
}

nsresult
nsImageDocument::StartLayout()
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

nsresult nsImageDocument::UpdateTitle()
{
  if (mStringBundle) {
    nsXPIDLString fileStr;
    nsAutoString widthStr;
    nsAutoString heightStr;
    nsAutoString typeStr;
    nsXPIDLString valUni;

    nsCOMPtr<nsIURL> url = do_QueryInterface(mDocumentURL);
    if (url) {
      nsCAutoString fileName;
      url->GetFileName(fileName);
      fileStr.Assign(NS_ConvertUTF8toUCS2(fileName));
    }

    widthStr.AppendInt(mImageWidth);
    heightStr.AppendInt(mImageHeight);

    if (mImageRequest) {
      nsXPIDLCString mimeType;
      mImageRequest->GetMimeType(getter_Copies(mimeType));
      ToUpperCase(mimeType);
      nsXPIDLCString::const_iterator start, end;
      mimeType.BeginReading(start);
      mimeType.EndReading(end);
      nsXPIDLCString::const_iterator iter = end;
      if (FindInReadable(NS_LITERAL_CSTRING("IMAGE/"), start, iter) && iter != end) {
        // strip out "X-" if any
        if (*iter == 'X') {
          ++iter;
          if (iter != end && *iter == '-') {
            ++iter;
            if (iter == end) {
              // looks like "IMAGE/X-" is the type??  Bail out of here.
              mimeType.BeginReading(iter);
            }
          } else {
            --iter;
          }
        }
        CopyASCIItoUCS2(Substring(iter, end), typeStr);
      } else {
        CopyASCIItoUCS2(mimeType, typeStr);
      }
    }

    // If we got a filename, display it
    if (!fileStr.IsEmpty()) {
      // if we got a valid size (sometimes we do not) then display it
      if (mImageWidth != 0 && mImageHeight != 0){
        const PRUnichar *formatStrings[4]  = {fileStr.get(), typeStr.get(), widthStr.get(), heightStr.get()};
        mStringBundle->FormatStringFromName(NS_LITERAL_STRING("ImageTitleWithDimensionsAndFile").get(), formatStrings, 4, getter_Copies(valUni));
      } else {
        const PRUnichar *formatStrings[2] = {fileStr.get(), typeStr.get()};
        mStringBundle->FormatStringFromName(NS_LITERAL_STRING("ImageTitleWithoutDimensions").get(), formatStrings, 2, getter_Copies(valUni));
      }
    } else {
      // if we got a valid size (sometimes we do not) then display it
      if (mImageWidth != 0 && mImageHeight != 0){
        const PRUnichar *formatStrings[3]  = {typeStr.get(), widthStr.get(), heightStr.get()};
        mStringBundle->FormatStringFromName(NS_LITERAL_STRING("ImageTitleWithDimensions").get(), formatStrings, 3, getter_Copies(valUni));
      } else {
        const PRUnichar *formatStrings[1]  = {typeStr.get()};
        mStringBundle->FormatStringFromName(NS_LITERAL_STRING("ImageTitleWithNeitherDimensionsNorFile").get(), formatStrings, 1, getter_Copies(valUni));
      }
    }

    if (valUni) {
      // set it on the document
      SetTitle(valUni);
    }
  }

  return NS_OK;
}


nsresult
NS_NewImageDocument(nsIDocument** aResult)
{
  nsImageDocument* doc = new nsImageDocument();
  if (!doc) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    delete doc;
    return rv;
  }

  NS_ADDREF(*aResult = doc);

  return NS_OK;
}
