/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioEncoder_h
#define mozilla_dom_AudioEncoder_h

#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/EncoderTemplate.h"
#include "mozilla/dom/AudioData.h"
#include "nsCycleCollectionParticipant.h"
#include "EncoderTypes.h"
#include "EncoderAgent.h"

class nsIGlobalObject;

namespace mozilla::dom {

class AudioDataOutputCallback;
class EncodedAudioChunk;
class EncodedAudioChunkData;
class EventHandlerNonNull;
class GlobalObject;
class Promise;
class WebCodecsErrorCallback;
struct AudioEncoderConfig;
struct AudioEncoderInit;

}  // namespace mozilla::dom

namespace mozilla::dom {

class AudioEncoder final : public EncoderTemplate<AudioEncoderTraits> {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioEncoder, DOMEventTargetHelper)

 public:
  AudioEncoder(nsIGlobalObject* aParent,
               RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
               RefPtr<EncodedAudioChunkOutputCallback>&& aOutputCallback);

 protected:
  ~AudioEncoder();

 public:
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<AudioEncoder> Constructor(
      const GlobalObject& aGlobal, const AudioEncoderInit& aInit,
      ErrorResult& aRv);

  static already_AddRefed<Promise> IsConfigSupported(
      const GlobalObject& aGlobal, const AudioEncoderConfig& aConfig,
      ErrorResult& aRv);

 protected:
  virtual RefPtr<EncodedAudioChunk> EncodedDataToOutputType(
      nsIGlobalObject* aGlobalObject,
      const RefPtr<MediaRawData>& aData) override;

  virtual void EncoderConfigToDecoderConfig(
      JSContext* aCx, const RefPtr<MediaRawData>& aRawData,
      const AudioEncoderConfigInternal& aSrcConfig,
      AudioDecoderConfig& aDestConfig) const override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AudioEncoder_h
