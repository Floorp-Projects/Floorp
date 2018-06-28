/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {

MediaDocumentStreamListener::MediaDocumentStreamListener(MediaDocument *aDocument)
  : mDocument(aDocument)
{
}

MediaDocumentStreamListener::~MediaDocumentStreamListener()
{
  if (mDocument && !NS_IsMainThread()) {
    nsCOMPtr<nsIEventTarget> mainTarget(do_GetMainThread());
    NS_ProxyRelease("MediaDocumentStreamListener::mDocument",
                    mainTarget, mDocument.forget());
  }
}


NS_IMPL_ISUPPORTS(MediaDocumentStreamListener,
                  nsIRequestObserver,
                  nsIStreamListener,
                  nsIThreadRetargetableStreamListener)


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

  return NS_ERROR_PARSED_DATA_CACHED;
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

NS_IMETHODIMP
MediaDocumentStreamListener::CheckListenerChain()
{
  nsCOMPtr<nsIThreadRetargetableStreamListener> retargetable =
    do_QueryInterface(mNextStream);
  if (retargetable) {
    return retargetable->CheckListenerChain();
  }
  return NS_ERROR_NO_INTERFACE;
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
      mDidInitialDocumentSetup(false)
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

  const Encoding* encoding;
  int32_t source;
  nsCOMPtr<nsIPrincipal> principal;
  // opening in a new tab
  docShell->GetParentCharset(encoding, &source, getter_AddRefs(principal));

  if (encoding && encoding != UTF_8_ENCODING &&
      NodePrincipal()->Equals(principal)) {
    SetDocumentCharacterSetSource(source);
    SetDocumentCharacterSet(WrapNotNull(encoding));
  }

  return NS_OK;
}

void
MediaDocument::InitialSetupDone()
{
  MOZ_ASSERT(GetReadyStateEnum() == nsIDocument::READYSTATE_LOADING,
             "Bad readyState: we should still be doing our initial load");
  mDidInitialDocumentSetup = true;
  nsContentUtils::AddScriptRunner(
    new nsDocElementCreatedNotificationRunner(this));
  SetReadyStateInternal(nsIDocument::READYSTATE_INTERACTIVE);
}

nsresult
MediaDocument::CreateSyntheticDocument()
{
  MOZ_ASSERT(!InitialSetupHasBeenDone());

  // Synthesize an empty html document
  nsresult rv;

  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::html, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsINode::ELEMENT_NODE);

  RefPtr<nsGenericHTMLElement> root = NS_NewHTMLHtmlElement(nodeInfo.forget());
  NS_ENSURE_TRUE(root, NS_ERROR_OUT_OF_MEMORY);

  NS_ASSERTION(GetChildCount() == 0, "Shouldn't have any kids");
  rv = AppendChildTo(root, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::head, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsINode::ELEMENT_NODE);

  // Create a <head> so our title has somewhere to live
  RefPtr<nsGenericHTMLElement> head = NS_NewHTMLHeadElement(nodeInfo.forget());
  NS_ENSURE_TRUE(head, NS_ERROR_OUT_OF_MEMORY);

  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::meta, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsINode::ELEMENT_NODE);

  RefPtr<nsGenericHTMLElement> metaContent = NS_NewHTMLMetaElement(nodeInfo.forget());
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
                                           nsINode::ELEMENT_NODE);

  RefPtr<nsGenericHTMLElement> body = NS_NewHTMLBodyElement(nodeInfo.forget());
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
    nsresult rv = shell->Initialize();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
MediaDocument::GetFileName(nsAString& aResult, nsIChannel* aChannel)
{
  aResult.Truncate();

  if (aChannel) {
    aChannel->GetContentDispositionFilename(aResult);
    if (!aResult.IsEmpty())
      return;
  }

  nsCOMPtr<nsIURL> url = do_QueryInterface(mDocumentURI);
  if (!url)
    return;

  nsAutoCString fileName;
  url->GetFileName(fileName);
  if (fileName.IsEmpty())
    return;

  nsAutoCString docCharset;
  // Now that the charset is set in |StartDocumentLoad| to the charset of
  // the document viewer instead of a bogus value ("windows-1252" set in
  // |nsDocument|'s ctor), the priority is given to the current charset.
  // This is necessary to deal with a media document being opened in a new
  // window or a new tab.
  if (mCharacterSetSource != kCharsetUninitialized) {
    mCharacterSet->Name(docCharset);
  } else {
    // resort to UTF-8
    SetDocumentCharacterSet(UTF_8_ENCODING);
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
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::link, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsINode::ELEMENT_NODE);

  RefPtr<nsGenericHTMLElement> link = NS_NewHTMLLinkElement(nodeInfo.forget());
  NS_ENSURE_TRUE(link, NS_ERROR_OUT_OF_MEMORY);

  link->SetAttr(kNameSpaceID_None, nsGkAtoms::rel,
                NS_LITERAL_STRING("stylesheet"), true);

  link->SetAttr(kNameSpaceID_None, nsGkAtoms::href, aStylesheet, true);

  Element* head = GetHeadElement();
  return head->AppendChildTo(link, false);
}

nsresult
MediaDocument::LinkScript(const nsAString& aScript)
{
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::script, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsINode::ELEMENT_NODE);

  RefPtr<nsGenericHTMLElement> script = NS_NewHTMLScriptElement(nodeInfo.forget());
  NS_ENSURE_TRUE(script, NS_ERROR_OUT_OF_MEMORY);

  script->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                  NS_LITERAL_STRING("text/javascript"), true);

  script->SetAttr(kNameSpaceID_None, nsGkAtoms::src, aScript, true);

  Element* head = GetHeadElement();
  return head->AppendChildTo(script, false);
}

void
MediaDocument::UpdateTitleAndCharset(const nsACString& aTypeStr,
                                     nsIChannel* aChannel,
                                     const char* const* aFormatNames,
                                     int32_t aWidth, int32_t aHeight,
                                     const nsAString& aStatus)
{
  nsAutoString fileStr;
  GetFileName(fileStr, aChannel);

  NS_ConvertASCIItoUTF16 typeStr(aTypeStr);
  nsAutoString title;

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
        mStringBundle->FormatStringFromName(aFormatNames[eWithDimAndFile],
                                            formatStrings, 4, title);
      }
      else {
        const char16_t *formatStrings[3]  = {typeStr.get(), widthStr.get(),
          heightStr.get()};
        mStringBundle->FormatStringFromName(aFormatNames[eWithDim],
                                            formatStrings, 3, title);
      }
    }
    else {
    // If we got a filename, display it
      if (!fileStr.IsEmpty()) {
        const char16_t *formatStrings[2] = {fileStr.get(), typeStr.get()};
        mStringBundle->FormatStringFromName(aFormatNames[eWithFile],
                                            formatStrings, 2, title);
      }
      else {
        const char16_t *formatStrings[1] = {typeStr.get()};
        mStringBundle->FormatStringFromName(aFormatNames[eWithNoInfo],
                                            formatStrings, 1, title);
      }
    }
  }

  // set it on the document
  if (aStatus.IsEmpty()) {
    IgnoredErrorResult ignored;
    SetTitle(title, ignored);
  }
  else {
    nsAutoString titleWithStatus;
    const nsPromiseFlatString& status = PromiseFlatString(aStatus);
    const char16_t *formatStrings[2] = {title.get(), status.get()};
    mStringBundle->FormatStringFromName("TitleWithStatus", formatStrings,
                                        2, titleWithStatus);
    IgnoredErrorResult ignored;
    SetTitle(titleWithStatus, ignored);
  }
}

} // namespace dom
} // namespace mozilla
