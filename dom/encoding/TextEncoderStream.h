/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_ENCODING_TEXTENCODERSTREAM_H_
#define DOM_ENCODING_TEXTENCODERSTREAM_H_

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class Decoder;

namespace dom {

class ReadableStream;
class WritableStream;
class TransformStream;

}  // namespace dom

}  // namespace mozilla

namespace mozilla::dom {

class TextEncoderStream final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TextEncoderStream)

 public:
  TextEncoderStream(nsISupports* aGlobal, TransformStream& aStream);

  nsISupports* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  mozilla::Decoder* Decoder() { return mDecoder.get(); }

  // TODO: Mark as MOZ_CAN_RUN_SCRIPT when IDL constructors can be (bug 1749042)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static already_AddRefed<TextEncoderStream>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  ReadableStream* Readable() const;

  WritableStream* Writable() const;

  void GetEncoding(nsCString& aRetVal) const;

 private:
  ~TextEncoderStream();

  nsCOMPtr<nsISupports> mGlobal;
  RefPtr<TransformStream> mStream;
  mozilla::UniquePtr<mozilla::Decoder> mDecoder;
};

}  // namespace mozilla::dom

#endif  // DOM_ENCODING_TEXTENCODERSTREAM_H_
