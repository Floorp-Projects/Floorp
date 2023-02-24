
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadRequest_h
#define mozilla_dom_ReadRequest_h

#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/LinkedList.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

// List Owners must traverse this class.
struct ReadRequest : public nsISupports,
                     public LinkedListElement<RefPtr<ReadRequest>> {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ReadRequest)

  // PipeToReadRequest::ChunkSteps can run script, for example.
  MOZ_CAN_RUN_SCRIPT virtual void ChunkSteps(JSContext* aCx,
                                             JS::Handle<JS::Value> aChunk,
                                             ErrorResult& aRv) = 0;
  MOZ_CAN_RUN_SCRIPT virtual void CloseSteps(JSContext* aCx,
                                             ErrorResult& aRv) = 0;
  virtual void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> e,
                          ErrorResult& aRv) = 0;

 protected:
  virtual ~ReadRequest() = default;
};

}  // namespace mozilla::dom

#endif
