/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDocument.h"
#include "nsGkAtoms.h"
#include "nsRect.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIScrollable.h"
#include "nsViewManager.h"
#include "nsITextToSubURI.h"
#include "nsIURL.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsCharsetSource.h" // kCharsetFrom* macro definition
#include "nsNodeInfoManager.h"
#include "nsContentUtils.h"
#include "nsDocElementCreatedNotificationRunner.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrincipal.h"
#include "nsIMultiPartChannel.h"

namespace mozilla {
namespace dom {

MediaDocumentStreamListener::MediaDocumentStreamListener(MediaDocument *aDocument)
{
  mDocument = aDocument;
}

MediaDocumentStreamListener::~MediaDocumentStreamListener()
{
}


NS_IMPL_ISUPPORTS(MediaDocumentStreamListener,
                  nsIRequestObserver,
                  nsIStreamListener)


void
MediaDocumentStreamListener::SetStreamListener(nsIStreamListener *aListener)
{
  mNextStream = aListener;
}

NS_IMETHODIMP
MediaDocumentStreamListener::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  mDocument->StartLayout();

  if (mNextStream) {
    return mNextStream->OnStartRequest(request, ctxt);
  }

  return NS_BINDING_ABORTED;
}

NS_IMETHODIMP
MediaDocumentStreamListener::OnStopRequest(nsIRequest* request,
                                           nsISupports *ctxt,
                                           nsresult status)
{
  nsresult rv = NS_OK;
  if (mNextStream) {
    rv = mNextStream->OnStopRequest(request, ctxt, status);
  }

  // Don't release mDocument here if we're in the middle of a multipart response.
  bool lastPart = true;
  nsCOMPtr<nsIMultiPartChannel> mpchan(do_QueryInterface(request));
  if (mpchan) {
    mpchan->GetIsLastPart(&lastPart);
  }

  if (lastPart) {
    mDocument = nullptr;
  }
  return rv;
}

NS_IMETHODIMP
MediaDocumentStreamListener::OnDataAvailable(nsIRequest* request,
                                             nsISupports *ctxt,
                                             nsIInputStream *inStr,
                                             uint64_t sourceOffset,
                                             uint32_t count)
{
  if (mNextStream) {
    return mNextStream->OnDataAvailable(request, ctxt, inStr, sourceOffset, count);
  }

  return NS_OK;
}

// default format names for MediaDocument. 
const char* const MediaDocument::sFormatNames[4] = 
{
  "MediaTitleWithNoInfo",    // eWithNoInfo
  "MediaTitleWithFile",      // eWithFile
  "",                        // eWithDim
  ""                         // eWithDimAndFile
};

MediaDocument::MediaDocument()
    : nsHTMLDocument(),
      mDocumentElementInserted(false)
{
}
MediaDocument::~MediaDocument()
{
}

nsresult
MediaDocument::Init()
{
  nsresult rv = nsHTMLDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a bundle for the localization
  nsCOMPtr<nsIStringBundleService> stringService =
    mozilla::services::GetStringBundleService();
  if (stringService) {
    stringService->CreateBundle(NSMEDIADOCUMENT_PROPERTIES_URI,
                                getter_AddRefs(mStringBundle));
  }

  mIsSyntheticDocument = true;

  return NS_OK;
}

nsresult
MediaDocument::StartDocumentLoad(const char*         aCommand,
                                 nsIChannel*         aChannel,
                                 nsILoadGroup*       aLoadGroup,
                                 nsISupports*        aContainer,
                                 nsIStreamListener** aDocListener,
                                 bool                aReset,
                                 nsIContentSink*     aSink)
{
  nsresult rv = nsDocument::StartDocumentLoad(aCommand, aChannel, aLoadGroup,
                                              aContainer, aDocListener, aReset,
                                              aSink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We try to set the charset of the current document to that of the 
  // 'genuine' (as opposed to an intervening 'chrome') parent document 
  // that may be in a different window/tab. Even if we fail here,
  // we just return NS_OK because another attempt is made in 
  // |UpdateTitleAndCharset| and the worst thing possible is a mangled 
  // filename in the titlebar and the file picker.

  // Note that we
  // exclude UTF-8 as 'invalid' because UTF-8 is likely to be the charset 
  // of a chrome document that has nothing to do with the actual content 
  // whose charset we want to know. Even if "the actual content" is indeed 
  // in UTF-8, we don't lose anything because the default empty value is 
  // considered synonymous with UTF-8. 
    
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));

  // not being able to set the charset is not critical.
  NS_ENSURE_TRUE(docShell, NS_OK); 

  nsAutoCString charset;
  int32_t source;
  nsCOMPtr<nsIPrincipal> principal;
  // opening in a new tab
  docShell->GetParentCharset(charset, &source, getter_AddRefs(principal));

  if (!charset.IsEmpty() &&
      !charset.EqualsLiteral("UTF-8") &&
      NodePrincipal()->Equals(principal)) {
    SetDocumentCharacterSetSource(source);
    SetDocumentCharacterSet(charset);
  }

  return NS_OK;
}

void
MediaDocument::BecomeInteractive()
{
  // In principle, if we knew the readyState code to work, we could infer
  // restoration from GetReadyStateEnum() == nsIDocument::READYSTATE_COMPLETE.
  bool restoring = false;
  nsPIDOMWindow* window = GetWindow();
  if (window) {
    nsIDocShell* docShell = window->GetDocShell();
    if (docShell) {
      docShell->GetRestoringDocument(&restoring);
    }
  }
  if (!restoring) {
    MOZ_ASSERT(GetReadyStateEnum() == nsIDocument::READYSTATE_LOADING,
               "Bad readyState");
    SetReadyStateInternal(nsIDocument::READYSTATE_INTERACTIVE);
  }
}

nsresult
MediaDocument::CreateSyntheticDocument()
{
  // Synthesize an empty html document
  nsresult rv;

  nsRefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::html, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<nsGenericHTMLElement> root = NS_NewHTMLHtmlElement(nodeInfo.forget());
  NS_ENSURE_TRUE(root, NS_ERROR_OUT_OF_MEMORY);

  NS_ASSERTION(GetChildCount() == 0, "Shouldn't have any kids");
  rv = AppendChildTo(root, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::head, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);

  // Create a <head> so our title has somewhere to live
  nsRefPtr<nsGenericHTMLElement> head = NS_NewHTMLHeadElement(nodeInfo.forget());
  NS_ENSURE_TRUE(head, NS_ERROR_OUT_OF_MEMORY);

  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::meta, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<nsGenericHTMLElement> metaContent = NS_NewHTMLMetaElement(nodeInfo.forget());
  NS_ENSURE_TRUE(metaContent, NS_ERROR_OUT_OF_MEMORY);
  metaContent->SetAttr(kNameSpaceID_None, nsGkAtoms::name,
                       NS_LITERAL_STRING("viewport"),
                       true);

  metaContent->SetAttr(kNameSpaceID_None, nsGkAtoms::content,
                       NS_LITERAL_STRING("width=device-width; height=device-height;"),
                       true);
  head->AppendChildTo(metaContent, false);

  root->AppendChildTo(head, false);

  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::body, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<nsGenericHTMLElement> body = NS_NewHTMLBodyElement(nodeInfo.forget());
  NS_ENSURE_TRUE(body, NS_ERROR_OUT_OF_MEMORY);

  root->AppendChildTo(body, false);

  return NS_OK;
}

nsresult
MediaDocument::StartLayout()
{
  mMayStartLayout = true;
  nsCOMPtr<nsIPresShell> shell = GetShell();
  // Don't mess with the presshell if someone has already handled
  // its initial reflow.
  if (shell && !shell->DidInitialize()) {
    nsRect visibleArea = shell->GetPresContext()->GetVisibleArea();
    nsresult rv = shell->Initialize(visibleArea.width, visibleArea.height);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
MediaDocument::GetFileName(nsAString& aResult)
{
  aResult.Truncate();

  nsCOMPtr<nsIURL> url = do_QueryInterface(mDocumentURI);
  if (!url)
    return;

  nsAutoCString fileName;
  url->GetFileName(fileName);
  if (fileName.IsEmpty())
    return;

  nsAutoCString docCharset;
  // Now that the charset is set in |StartDocumentLoad| to the charset of
  // the document viewer instead of a bogus value ("ISO-8859-1" set in
  // |nsDocument|'s ctor), the priority is given to the current charset. 
  // This is necessary to deal with a media document being opened in a new 
  // window or a new tab, in which case |originCharset| of |nsIURI| is not 
  // reliable.
  if (mCharacterSetSource != kCharsetUninitialized) {  
    docCharset = mCharacterSet;
  } else {  
    // resort to |originCharset|
    url->GetOriginCharset(docCharset);
    SetDocumentCharacterSet(docCharset);
  }

  nsresult rv;
  nsCOMPtr<nsITextToSubURI> textToSubURI = 
    do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    // UnEscapeURIForUI always succeeds
    textToSubURI->UnEscapeURIForUI(docCharset, fileName, aResult);
  } else {
    CopyUTF8toUTF16(fileName, aResult);
  }
}

nsresult
MediaDocument::LinkStylesheet(const nsAString& aStylesheet)
{
  nsRefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::link, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<nsGenericHTMLElement> link = NS_NewHTMLLinkElement(nodeInfo.forget());
  NS_ENSURE_TRUE(link, NS_ERROR_OUT_OF_MEMORY);

  link->SetAttr(kNameSpaceID_None, nsGkAtoms::rel, 
                NS_LITERAL_STRING("stylesheet"), true);

  link->SetAttr(kNameSpaceID_None, nsGkAtoms::href, aStylesheet, true);

  Element* head = GetHeadElement();
  return head->AppendChildTo(link, false);
}

void 
MediaDocument::UpdateTitleAndCharset(const nsACString& aTypeStr,
                                     const char* const* aFormatNames,
                                     int32_t aWidth, int32_t aHeight,
                                     const nsAString& aStatus)
{
  nsXPIDLString fileStr;
  GetFileName(fileStr);

  NS_ConvertASCIItoUTF16 typeStr(aTypeStr);
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
        const char16_t *formatStrings[4]  = {fileStr.get(), typeStr.get(), 
          widthStr.get(), heightStr.get()};
        NS_ConvertASCIItoUTF16 fmtName(aFormatNames[eWithDimAndFile]);
        mStringBundle->FormatStringFromName(fmtName.get(), formatStrings, 4,
                                            getter_Copies(title));
      } 
      else {
        const char16_t *formatStrings[3]  = {typeStr.get(), widthStr.get(), 
          heightStr.get()};
        NS_ConvertASCIItoUTF16 fmtName(aFormatNames[eWithDim]);
        mStringBundle->FormatStringFromName(fmtName.get(), formatStrings, 3,
                                            getter_Copies(title));
      }
    } 
    else {
    // If we got a filename, display it
      if (!fileStr.IsEmpty()) {
        const char16_t *formatStrings[2] = {fileStr.get(), typeStr.get()};
        NS_ConvertASCIItoUTF16 fmtName(aFormatNames[eWithFile]);
        mStringBundle->FormatStringFromName(fmtName.get(), formatStrings, 2,
                                            getter_Copies(title));
      }
      else {
        const char16_t *formatStrings[1] = {typeStr.get()};
        NS_ConvertASCIItoUTF16 fmtName(aFormatNames[eWithNoInfo]);
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
    const char16_t *formatStrings[2] = {title.get(), status.get()};
    NS_NAMED_LITERAL_STRING(fmtName, "TitleWithStatus");
    mStringBundle->FormatStringFromName(fmtName.get(), formatStrings, 2,
                                        getter_Copies(titleWithStatus));
    SetTitle(titleWithStatus);
  }
}

void 
MediaDocument::SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject)
{
    nsHTMLDocument::SetScriptGlobalObject(aGlobalObject);
    if (!mDocumentElementInserted && aGlobalObject) {
        mDocumentElementInserted = true;
        nsContentUtils::AddScriptRunner(
            new nsDocElementCreatedNotificationRunner(this));        
    }
}

} // namespace dom
} // namespace mozilla
