
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadIntoRequest_h
#define mozilla_dom_ReadIntoRequest_h

#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/LinkedList.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

// This list element needs to be cycle collected by owning structures.
struct ReadIntoRequest : public nsISupports,
                         public LinkedListElement<RefPtr<ReadIntoRequest>> {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ReadIntoRequest)

  // An algorithm taking a chunk, called when a chunk is available for reading
  virtual void ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                          ErrorResult& aRv) = 0;

  // An algorithm taking a chunk or undefined, called when no chunks are
  // available because the stream is closed
  MOZ_CAN_RUN_SCRIPT
  virtual void CloseSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                          ErrorResult& aRv) = 0;

  // An algorithm taking a JavaScript value, called when no chunks are available
  // because the stream is errored
  virtual void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> e,
                          ErrorResult& aRv) = 0;

 protected:
  virtual ~ReadIntoRequest() = default;
};

}  // namespace mozilla::dom

#endif
