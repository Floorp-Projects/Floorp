/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_COMPRESSIONSTREAM_H_
#define DOM_COMPRESSIONSTREAM_H_

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class ReadableStream;
class WritableStream;
class TransformStream;

enum class CompressionFormat : uint8_t;

class CompressionStream final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(CompressionStream)

 public:
  CompressionStream(nsISupports* aGlobal, TransformStream& aStream);

 protected:
  ~CompressionStream();

 public:
  nsISupports* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // TODO: Mark as MOZ_CAN_RUN_SCRIPT when IDL constructors can be (bug 1749042)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static already_AddRefed<CompressionStream>
  Constructor(const GlobalObject& aGlobal, CompressionFormat aFormat,
              ErrorResult& aRv);

  already_AddRefed<ReadableStream> Readable() const;

  already_AddRefed<WritableStream> Writable() const;

 private:
  nsCOMPtr<nsISupports> mGlobal;

  RefPtr<TransformStream> mStream;
};

}  // namespace mozilla::dom

#endif  // DOM_COMPRESSIONSTREAM_H_
