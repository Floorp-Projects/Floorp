/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SourceBufferList_h_
#define mozilla_dom_SourceBufferList_h_

#include "AsyncEventRunner.h"
#include "MediaSource.h"
#include "SourceBuffer.h"
#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"
#include "nsWrapperCache.h"
#include "nscore.h"

namespace mozilla {

class ErrorResult;
template <typename T> class AsyncEventRunner;

namespace dom {

class MediaSource;

class SourceBufferList MOZ_FINAL : public nsDOMEventTargetHelper
{
public:
  /** WebIDL Methods. */
  SourceBuffer* IndexedGetter(uint32_t aIndex, bool& aFound);

  uint32_t Length();
  /** End WebIDL methods. */

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SourceBufferList, nsDOMEventTargetHelper)

  explicit SourceBufferList(MediaSource* aMediaSource);

  MediaSource* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

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

  // Detach and remove all SourceBuffers and fire a single "removesourcebuffer" at the list.
  void DetachAndClear();

  // Returns true if updating is true on any SourceBuffers in the list.
  bool AnyUpdating();

  // Calls Remove(aStart, aEnd) on each SourceBuffer in the list.  Aborts on
  // first error, with result returned in aRv.
  void Remove(double aStart, double aEnd, ErrorResult& aRv);

private:
  friend class AsyncEventRunner<SourceBufferList>;
  void DispatchSimpleEvent(const char* aName);
  void QueueAsyncSimpleEvent(const char* aName);

  nsRefPtr<MediaSource> mMediaSource;
  nsTArray<nsRefPtr<SourceBuffer> > mSourceBuffers;
};

} // namespace dom
} // namespace mozilla
#endif /* mozilla_dom_SourceBufferList_h_ */
