/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebVTTLoadListener.h"
#include "mozilla/dom/TextTrackCue.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "VideoUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_1(WebVTTLoadListener, mElement)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebVTTLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebVTTLoadListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebVTTLoadListener)

#ifdef PR_LOGGING
PRLogModuleInfo* gTextTrackLog;
# define LOG(...) PR_LOG(gTextTrackLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
# define LOG(msg)
#endif

WebVTTLoadListener::WebVTTLoadListener(HTMLTrackElement* aElement)
  : mElement(aElement)
{
  MOZ_ASSERT(mElement, "Must pass an element to the callback");
#ifdef PR_LOGGING
  if (!gTextTrackLog) {
    gTextTrackLog = PR_NewLogModule("TextTrack");
  }
#endif
  LOG("WebVTTLoadListener created.");
}

WebVTTLoadListener::~WebVTTLoadListener()
{
  LOG("WebVTTLoadListener destroyed.");
}

nsresult
WebVTTLoadListener::LoadResource()
{
  if (!HTMLTrackElement::IsWebVTTEnabled()) {
    NS_WARNING("WebVTT support disabled."
               " See media.webvtt.enabled in about:config. ");
    return NS_ERROR_FAILURE;
  }

  LOG("Loading text track resource.");
  webvtt_parser_t* parser = nullptr;
  webvtt_status status = webvtt_create_parser(&OnParsedCueWebVTTCallBack,
                                              &OnReportErrorWebVTTCallBack,
                                              this, &parser);

  if (status != WEBVTT_SUCCESS) {
    NS_ENSURE_TRUE(status != WEBVTT_OUT_OF_MEMORY,
                   NS_ERROR_OUT_OF_MEMORY);
    NS_ENSURE_TRUE(status != WEBVTT_INVALID_PARAM,
                   NS_ERROR_INVALID_ARG);
    return NS_ERROR_FAILURE;
  }

  mParser.own(parser);
  NS_ENSURE_TRUE(mParser != nullptr, NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
WebVTTLoadListener::OnStartRequest(nsIRequest* aRequest,
                                   nsISupports* aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
WebVTTLoadListener::OnStopRequest(nsIRequest* aRequest,
                                  nsISupports* aContext,
                                  nsresult aStatus)
{
  webvtt_finish_parsing(mParser);
  return NS_OK;
}

NS_IMETHODIMP
WebVTTLoadListener::OnDataAvailable(nsIRequest* aRequest,
                                    nsISupports* aContext,
                                    nsIInputStream* aStream,
                                    uint64_t aOffset,
                                    uint32_t aCount)
{
  uint32_t count = aCount;
  while (count > 0) {
    uint32_t read;
    nsresult rv = aStream->ReadSegments(ParseChunk, this, count, &read);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!read) {
      return NS_ERROR_FAILURE;
    }
    count -= read;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebVTTLoadListener::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                           nsIChannel* aNewChannel,
                                           uint32_t aFlags,
                                           nsIAsyncVerifyRedirectCallback* cb)
{
  if (mElement) {
    mElement->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP
WebVTTLoadListener::GetInterface(const nsIID &aIID,
                                 void** aResult)
{
  return QueryInterface(aIID, aResult);
}

NS_METHOD
WebVTTLoadListener::ParseChunk(nsIInputStream* aInStream, void* aClosure,
                               const char* aFromSegment, uint32_t aToOffset,
                               uint32_t aCount, uint32_t* aWriteCount)
{
  WebVTTLoadListener* loadListener = static_cast<WebVTTLoadListener*>(aClosure);

  if (WEBVTT_FAILED(webvtt_parse_chunk(loadListener->mParser, aFromSegment,
                                       aCount))) {
    LOG("Unable to parse chunk of WEBVTT text. Aborting.");
    *aWriteCount = 0;
    return NS_ERROR_FAILURE;
  }
  *aWriteCount = aCount;
  return NS_OK;
}

void
WebVTTLoadListener::OnParsedCue(webvtt_cue* aCue)
{
  const char* text = webvtt_string_text(&aCue->body);

  nsRefPtr<TextTrackCue> textTrackCue =
    new TextTrackCue(mElement->OwnerDoc()->GetParentObject(),
                     SECONDS_TO_MS(aCue->from), SECONDS_TO_MS(aCue->until),
                     NS_ConvertUTF8toUTF16(text), mElement,
                     aCue->node_head);

  text = webvtt_string_text(&aCue->id);
  textTrackCue->SetId(NS_ConvertUTF8toUTF16(text));

  textTrackCue->SetSnapToLines(aCue->snap_to_lines);
  textTrackCue->SetSize(aCue->settings.size);
  textTrackCue->SetPosition(aCue->settings.position);
  textTrackCue->SetLine(aCue->settings.line);

  nsAutoString vertical;
  switch (aCue->settings.vertical) {
    case WEBVTT_VERTICAL_LR:
      vertical = NS_LITERAL_STRING("lr");
      break;
    case WEBVTT_VERTICAL_RL:
      vertical = NS_LITERAL_STRING("rl");
      break;
    case WEBVTT_HORIZONTAL:
      // TODO: https://bugzilla.mozilla.org/show_bug.cgi?id=865407
      // Will be handled in the processing model.
      break;
  }
  textTrackCue->SetVertical(vertical);

  TextTrackCueAlign align;
  switch (aCue->settings.align) {
    case WEBVTT_ALIGN_START:
      align = TextTrackCueAlign::Start;
      break;
    case WEBVTT_ALIGN_MIDDLE:
      align = TextTrackCueAlign::Middle;
    case WEBVTT_ALIGN_END:
      align = TextTrackCueAlign::End;
    case WEBVTT_ALIGN_LEFT:
      align = TextTrackCueAlign::Left;
      break;
    case WEBVTT_ALIGN_RIGHT:
      align = TextTrackCueAlign::Right;
      break;
    default:
      align = TextTrackCueAlign::Start;
      break;
  }
  textTrackCue->SetAlign(align);

  mElement->mTrack->AddCue(*textTrackCue);
}

int
WebVTTLoadListener::OnReportError(uint32_t aLine, uint32_t aCol,
                                  webvtt_error aError)
{
#ifdef PR_LOGGING
  // Get source webvtt file to display in the log
  DOMString wideFile;
  mElement->GetSrc(wideFile);

  NS_ConvertUTF16toUTF8 file(wideFile);

  const char* error = "parser error";
  if (aError >= 0) {
    error = webvtt_strerror(aError);
  }

  LOG("error: %s(%d:%d) - %s\n", file.get(), aLine, aCol, error);
#endif

  switch(aError) {
    // Errors which should result in dropped cues
    // if the return value is negative:
    case WEBVTT_MALFORMED_TIMESTAMP:
      return -1;

    // By default, we can safely ignore other errors
    // or else parsing the document will be aborted regardless
    // of the return value.
    default:
      return 0;
  }
}

void WEBVTT_CALLBACK
WebVTTLoadListener::OnParsedCueWebVTTCallBack(void* aUserData, webvtt_cue* aCue)
{
  WebVTTLoadListener* self = static_cast<WebVTTLoadListener*>(aUserData);
  self->OnParsedCue(aCue);
}

int WEBVTT_CALLBACK
WebVTTLoadListener::OnReportErrorWebVTTCallBack(void* aUserData, uint32_t aLine,
                                                uint32_t aCol,
                                                webvtt_error aError)
{
  WebVTTLoadListener* self = static_cast<WebVTTLoadListener*>(aUserData);
  return self->OnReportError(aLine, aCol, aError);
}

} // namespace dom
} // namespace mozilla
