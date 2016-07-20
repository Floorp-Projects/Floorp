/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SourceBufferList_h_
#define mozilla_dom_SourceBufferList_h_

#include "SourceBuffer.h"
#include "js/RootingAPI.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsTArray.h"

struct JSContext;
class JSObject;

namespace mozilla {

template <typename T> class AsyncEventRunner;

namespace dom {

class MediaSource;

class SourceBufferList final : public DOMEventTargetHelper
{
public:
  /** WebIDL Methods. */
  SourceBuffer* IndexedGetter(uint32_t aIndex, bool& aFound);

  uint32_t Length();

  IMPL_EVENT_HANDLER(addsourcebuffer);
  IMPL_EVENT_HANDLER(removesourcebuffer);

  /** End WebIDL methods. */

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SourceBufferList,
                                           DOMEventTargetHelper)

  explicit SourceBufferList(MediaSource* aMediaSource);

  MediaSource* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // Append a SourceBuffer and fire "addsourcebuffer" at the list.
  void Append(SourceBuffer* aSourceBuffer);

  // Remove a SourceBuffer and fire "removesourcebuffer" at the list.
  void Remove(SourceBuffer* aSourceBuffer);

  // Returns true if aSourceBuffer is present in the list.
  bool Contains(SourceBuffer* aSourceBuffer);

  // Remove all SourceBuffers and fire a single "removesourcebuffer" at the list.
  void Clear();

  // True if list has zero entries.
  bool IsEmpty();

  // Returns true if updating is true on any SourceBuffers in the list.
  bool AnyUpdating();

  // Runs the range removal steps from the MSE specification on each SourceBuffer.
  void RangeRemoval(double aStart, double aEnd);

  // Mark all SourceBuffers input buffers as ended.
  void Ended();

  // Returns the highest end time of any of the Sourcebuffers.
  double GetHighestBufferedEndTime();

  // Append a SourceBuffer to the list. No event is fired.
  void AppendSimple(SourceBuffer* aSourceBuffer);

  // Remove all SourceBuffers from mSourceBuffers.
  //  No event is fired and no action is performed on the sourcebuffers.
  void ClearSimple();

  double HighestStartTime();

private:
  ~SourceBufferList();

  friend class AsyncEventRunner<SourceBufferList>;
  void DispatchSimpleEvent(const char* aName);
  void QueueAsyncSimpleEvent(const char* aName);

  RefPtr<MediaSource> mMediaSource;
  nsTArray<RefPtr<SourceBuffer> > mSourceBuffers;
};

} // namespace dom

} // namespace mozilla

#endif /* mozilla_dom_SourceBufferList_h_ */
