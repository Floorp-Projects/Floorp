/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsError.h"
#include "nsIDOMHTMLAudioElement.h"
#include "nsHTMLAudioElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "jsfriendapi.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "AudioSampleFormat.h"
#include "AudioChannelCommon.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

nsGenericHTMLElement*
NS_NewHTMLAudioElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                       FromParser aFromParser)
{
  /*
   * nsHTMLAudioElement's will be created without a nsINodeInfo passed in
   * if someone says "var audio = new Audio();" in JavaScript, in a case like
   * that we request the nsINodeInfo from the document's nodeinfo list.
   */
  nsCOMPtr<nsINodeInfo> nodeInfo(aNodeInfo);
  if (!nodeInfo) {
    nsCOMPtr<nsIDocument> doc =
      do_QueryInterface(nsContentUtils::GetDocumentFromCaller());
    NS_ENSURE_TRUE(doc, nullptr);

    nodeInfo = doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::audio, nullptr,
                                                   kNameSpaceID_XHTML,
                                                   nsIDOMNode::ELEMENT_NODE);
    NS_ENSURE_TRUE(nodeInfo, nullptr);
  }

  return new nsHTMLAudioElement(nodeInfo.forget());
}

NS_IMPL_ADDREF_INHERITED(nsHTMLAudioElement, nsHTMLMediaElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLAudioElement, nsHTMLMediaElement)

DOMCI_NODE_DATA(HTMLAudioElement, nsHTMLAudioElement)

NS_INTERFACE_TABLE_HEAD(nsHTMLAudioElement)
NS_HTML_CONTENT_INTERFACE_TABLE3(nsHTMLAudioElement, nsIDOMHTMLMediaElement,
                                 nsIDOMHTMLAudioElement, nsIJSNativeInitializer)
NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLAudioElement,
                                               nsHTMLMediaElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLAudioElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLAudioElement)


nsHTMLAudioElement::nsHTMLAudioElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsHTMLMediaElement(aNodeInfo)
{
}

nsHTMLAudioElement::~nsHTMLAudioElement()
{
}

NS_IMETHODIMP
nsHTMLAudioElement::Initialize(nsISupports* aOwner, JSContext* aContext,
                               JSObject *aObj, uint32_t argc, jsval *argv)
{
  // Audio elements created using "new Audio(...)" should have
  // 'preload' set to 'auto' (since the script must intend to
  // play the audio)
  nsresult rv = SetAttr(kNameSpaceID_None, nsGkAtoms::preload,
                        NS_LITERAL_STRING("auto"), true);
  if (NS_FAILED(rv))
    return rv;

  if (argc <= 0) {
    // Nothing more to do here if we don't get any arguments.
    return NS_OK;
  }

  // The only (optional) argument is the url of the audio
  JSString* jsstr = JS_ValueToString(aContext, argv[0]);
  if (!jsstr)
    return NS_ERROR_FAILURE;

  nsDependentJSString str;
  if (!str.init(aContext, jsstr))
    return NS_ERROR_FAILURE;

  // The only (optional) argument is the src of the audio (which must
  // be a URL string), used to initialize the 'src' attribute.
  return SetSrc(str);
}

NS_IMETHODIMP
nsHTMLAudioElement::MozSetup(uint32_t aChannels, uint32_t aRate)
{
  // If there is already a src provided, don't setup another stream
  if (mDecoder) {
    return NS_ERROR_FAILURE;
  }

  // MozWriteAudio divides by mChannels, so validate now.
  if (0 == aChannels) {
    return NS_ERROR_FAILURE;
  }

  if (mAudioStream) {
    mAudioStream->Shutdown();
  }

  mAudioStream = AudioStream::AllocateStream();
  nsresult rv = mAudioStream->Init(aChannels, aRate, mAudioChannelType);
  if (NS_FAILED(rv)) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
    return rv;
  }

  MetadataLoaded(aChannels, aRate, true, nullptr);
  mAudioStream->SetVolume(mVolume);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLAudioElement::MozWriteAudio(const JS::Value& aData, JSContext* aCx, uint32_t* aRetVal)
{
  if (!mAudioStream) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (!aData.isObject()) {
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  }

  JSObject* darray = &aData.toObject();
  JS::AutoObjectRooter tvr(aCx);
  JSObject* tsrc = NULL;

  // Allow either Float32Array or plain JS Array
  if (JS_IsFloat32Array(darray)) {
    tsrc = darray;
  } else if (JS_IsArrayObject(aCx, darray)) {
    JSObject* nobj = JS_NewFloat32ArrayFromArray(aCx, darray);
    if (!nobj) {
      return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
    }
    tsrc = nobj;
  } else {
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  }
  tvr.setObject(tsrc);

  uint32_t dataLength = JS_GetTypedArrayLength(tsrc);

  // Make sure that we are going to write the correct amount of data based
  // on number of channels.
  if (dataLength % mChannels != 0) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Don't write more than can be written without blocking.
  uint32_t writeLen = std::min(mAudioStream->Available(), dataLength / mChannels);

  float* frames = JS_GetFloat32ArrayData(tsrc);
  // Convert the samples back to integers as we are using fixed point audio in
  // the AudioStream.
  // This could be optimized to avoid allocation and memcpy when
  // AudioDataValue is 'float', but it's not worth it for this deprecated API.
  nsAutoArrayPtr<AudioDataValue> audioData(new AudioDataValue[writeLen * mChannels]);
  ConvertAudioSamples(frames, audioData.get(), writeLen * mChannels);
  nsresult rv = mAudioStream->Write(audioData.get(), writeLen);

  if (NS_FAILED(rv)) {
    return rv;
  }

  // Return the actual amount written.
  *aRetVal = writeLen * mChannels;
  return rv;
}

NS_IMETHODIMP
nsHTMLAudioElement::MozCurrentSampleOffset(uint64_t *aRetVal)
{
  if (!mAudioStream) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  int64_t position = mAudioStream->GetPositionInFrames();
  if (position < 0) {
    *aRetVal = 0;
  } else {
    *aRetVal = position * mChannels;
  }
  return NS_OK;
}

nsresult nsHTMLAudioElement::SetAcceptHeader(nsIHttpChannel* aChannel)
{
    nsAutoCString value(
#ifdef MOZ_WEBM
      "audio/webm,"
#endif
#ifdef MOZ_OGG
      "audio/ogg,"
#endif
#ifdef MOZ_WAVE
      "audio/wav,"
#endif
      "audio/*;q=0.9,"
#ifdef MOZ_OGG
      "application/ogg;q=0.7,"
#endif
      "video/*;q=0.6,*/*;q=0.5");

    return aChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      value,
                                      false);
}
