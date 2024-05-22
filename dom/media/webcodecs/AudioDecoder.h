/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioDecoder_h
#define mozilla_dom_AudioDecoder_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/AudioData.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DecoderTemplate.h"
#include "mozilla/dom/DecoderTypes.h"
#include "mozilla/dom/RootedDictionary.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

namespace dom {

class AudioDataOutputCallback;
class EncodedAudioChunk;
class EncodedAudioChunkData;
class EventHandlerNonNull;
class GlobalObject;
class Promise;
class WebCodecsErrorCallback;
struct AudioDecoderConfig;
struct AudioDecoderInit;

}  // namespace dom

}  // namespace mozilla

namespace mozilla::dom {

class AudioDecoder final : public DecoderTemplate<AudioDecoderTraits> {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioDecoder, DOMEventTargetHelper)

 public:
  AudioDecoder(nsIGlobalObject* aParent,
               RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
               RefPtr<AudioDataOutputCallback>&& aOutputCallback);

 protected:
  ~AudioDecoder();

 public:
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<AudioDecoder> Constructor(
      const GlobalObject& aGlobal, const AudioDecoderInit& aInit,
      ErrorResult& aRv);

  static already_AddRefed<Promise> IsConfigSupported(
      const GlobalObject& aGlobal, const AudioDecoderConfig& aConfig,
      ErrorResult& aRv);

 protected:
  virtual already_AddRefed<MediaRawData> InputDataToMediaRawData(
      UniquePtr<EncodedAudioChunkData>&& aData, TrackInfo& aInfo,
      const AudioDecoderConfigInternal& aConfig) override;

  virtual nsTArray<RefPtr<AudioData>> DecodedDataToOutputType(
      nsIGlobalObject* aGlobalObject, const nsTArray<RefPtr<MediaData>>&& aData,
      const AudioDecoderConfigInternal& aConfig) override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AudioDecoder_h
