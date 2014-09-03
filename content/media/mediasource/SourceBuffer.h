/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SourceBuffer_h_
#define mozilla_dom_SourceBuffer_h_

#include "MediaDecoderReader.h"
#include "MediaSource.h"
#include "js/RootingAPI.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/SourceBufferBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/mozalloc.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nscore.h"

class JSObject;
struct JSContext;

namespace mozilla {

class ContainerParser;
class ErrorResult;
class SourceBufferResource;
class SourceBufferDecoder;
template <typename T> class AsyncEventRunner;

namespace dom {

class TimeRanges;

class SourceBuffer MOZ_FINAL : public DOMEventTargetHelper
{
public:
  /** WebIDL Methods. */
  SourceBufferAppendMode Mode() const
  {
    return mAppendMode;
  }

  void SetMode(SourceBufferAppendMode aMode, ErrorResult& aRv);

  bool Updating() const
  {
    return mUpdating;
  }

  already_AddRefed<TimeRanges> GetBuffered(ErrorResult& aRv);

  double TimestampOffset() const
  {
    return mTimestampOffset;
  }

  void SetTimestampOffset(double aTimestampOffset, ErrorResult& aRv);

  double AppendWindowStart() const
  {
    return mAppendWindowStart;
  }

  void SetAppendWindowStart(double aAppendWindowStart, ErrorResult& aRv);

  double AppendWindowEnd() const
  {
    return mAppendWindowEnd;
  }

  void SetAppendWindowEnd(double aAppendWindowEnd, ErrorResult& aRv);

  void AppendBuffer(const ArrayBuffer& aData, ErrorResult& aRv);
  void AppendBuffer(const ArrayBufferView& aData, ErrorResult& aRv);

  void Abort(ErrorResult& aRv);

  void Remove(double aStart, double aEnd, ErrorResult& aRv);
  /** End WebIDL Methods. */

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SourceBuffer, DOMEventTargetHelper)

  SourceBuffer(MediaSource* aMediaSource, const nsACString& aType);

  MediaSource* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // Notify the SourceBuffer that it has been detached from the
  // MediaSource's sourceBuffer list.
  void Detach();
  bool IsAttached() const
  {
    return mMediaSource != nullptr;
  }

  void Ended();

  // Evict data in the source buffer in the given time range.
  void Evict(double aStart, double aEnd);

  double GetBufferedStart();
  double GetBufferedEnd();

private:
  ~SourceBuffer();

  friend class AsyncEventRunner<SourceBuffer>;
  void DispatchSimpleEvent(const char* aName);
  void QueueAsyncSimpleEvent(const char* aName);

  // Create a new decoder for mType, and store the result in mDecoder.
  // Returns true if mDecoder was set.
  bool InitNewDecoder();

  // Set mDecoder to null and reset mDecoderInitialized.
  void DiscardDecoder();

  // Update mUpdating and fire the appropriate events.
  void StartUpdating();
  void StopUpdating();
  void AbortUpdating();

  // Shared implementation of AppendBuffer overloads.
  void AppendData(const uint8_t* aData, uint32_t aLength, ErrorResult& aRv);

  nsRefPtr<MediaSource> mMediaSource;

  const nsCString mType;

  nsAutoPtr<ContainerParser> mParser;

  double mLastParsedTimestamp;

  nsRefPtr<SourceBufferDecoder> mDecoder;
  nsTArray<nsRefPtr<SourceBufferDecoder>> mDecoders;

  double mAppendWindowStart;
  double mAppendWindowEnd;

  double mTimestampOffset;

  SourceBufferAppendMode mAppendMode;
  bool mUpdating;

  bool mDecoderInitialized;
};

} // namespace dom

} // namespace mozilla
#endif /* mozilla_dom_SourceBuffer_h_ */
