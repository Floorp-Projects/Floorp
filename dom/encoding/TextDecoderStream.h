/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_ENCODING_TEXTDECODERSTREAM_H_
#define DOM_ENCODING_TEXTDECODERSTREAM_H_

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/TextDecoder.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class Decoder;
class Encoding;

namespace dom {

class ReadableStream;
class WritableStream;
struct TextDecoderOptions;
class TransformStream;

}  // namespace dom

}  // namespace mozilla

namespace mozilla::dom {

class TextDecoderStream final : public nsISupports,
                                public nsWrapperCache,
                                public TextDecoderCommon {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TextDecoderStream)

 public:
  TextDecoderStream(nsISupports* aGlobal, const Encoding& aEncoding,
                    bool aFatal, bool aIgnoreBOM, TransformStream& aStream);

  nsISupports* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  mozilla::Decoder* Decoder() { return mDecoder.get(); }

  // TODO: Mark as MOZ_CAN_RUN_SCRIPT when IDL constructors can be (bug 1749042)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static already_AddRefed<TextDecoderStream>
  Constructor(const GlobalObject& aGlobal, const nsAString& aLabel,
              const TextDecoderOptions& aOptions, ErrorResult& aRv);

  ReadableStream* Readable() const;

  WritableStream* Writable() const;

 private:
  ~TextDecoderStream();

  nsCOMPtr<nsISupports> mGlobal;
  RefPtr<TransformStream> mStream;
};

}  // namespace mozilla::dom

#endif  // DOM_ENCODING_TEXTDECODERSTREAM_H_
