/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaSource_h_
#define mozilla_dom_MediaSource_h_

#include "MediaSourceDecoder.h"
#include "js/RootingAPI.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/MediaSourceBinding.h"
#include "mozilla/Monitor.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"
#include "nsISupports.h"
#include "nscore.h"

struct JSContext;
class JSObject;
class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;
template <typename T> class AsyncEventRunner;

namespace dom {

class GlobalObject;
class SourceBuffer;
class SourceBufferList;
template <typename T> class Optional;

#define MOZILLA_DOM_MEDIASOURCE_IMPLEMENTATION_IID \
  { 0x3839d699, 0x22c5, 0x439f, \
  { 0x94, 0xca, 0x0e, 0x0b, 0x26, 0xf9, 0xca, 0xbf } }

class MediaSource MOZ_FINAL : public DOMEventTargetHelper
{
public:
  /** WebIDL Methods. */
  static already_AddRefed<MediaSource>
  Constructor(const GlobalObject& aGlobal,
              ErrorResult& aRv);

  SourceBufferList* SourceBuffers();
  SourceBufferList* ActiveSourceBuffers();
  MediaSourceReadyState ReadyState();

  double Duration();
  void SetDuration(double aDuration, ErrorResult& aRv);

  already_AddRefed<SourceBuffer> AddSourceBuffer(const nsAString& aType, ErrorResult& aRv);
  void RemoveSourceBuffer(SourceBuffer& aSourceBuffer, ErrorResult& aRv);

  void EndOfStream(const Optional<MediaSourceEndOfStreamError>& aError, ErrorResult& aRv);
  static bool IsTypeSupported(const GlobalObject&, const nsAString& aType);
  /** End WebIDL Methods. */

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaSource, DOMEventTargetHelper)
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_MEDIASOURCE_IMPLEMENTATION_IID)

  nsPIDOMWindow* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // Attach this MediaSource to Decoder aDecoder.  Returns false if already attached.
  bool Attach(MediaSourceDecoder* aDecoder);
  void Detach();

  // Set mReadyState to aState and fire the required events at the MediaSource.
  void SetReadyState(MediaSourceReadyState aState);

 // Used by SourceBuffer to call CreateSubDecoder.
  MediaSourceDecoder* GetDecoder()
  {
    return mDecoder;
  }

  // Called by SourceBuffers to notify this MediaSource that data has
  // been evicted from the buffered data. The start and end times
  // that were evicted are provided.
  void NotifyEvicted(double aStart, double aEnd);

  // Block thread waiting for data to be appended to a SourceBuffer.
  void WaitForData();

  // Unblock threads waiting for data to be appended to a SourceBuffer.
  void NotifyGotData();

private:
  ~MediaSource();

  explicit MediaSource(nsPIDOMWindow* aWindow);

  friend class AsyncEventRunner<MediaSource>;
  void DispatchSimpleEvent(const char* aName);
  void QueueAsyncSimpleEvent(const char* aName);

  void DurationChange(double aNewDuration, ErrorResult& aRv);

  double mDuration;

  nsRefPtr<SourceBufferList> mSourceBuffers;
  nsRefPtr<SourceBufferList> mActiveSourceBuffers;

  nsRefPtr<MediaSourceDecoder> mDecoder;

  MediaSourceReadyState mReadyState;

  // Monitor for waiting for when new data is appended to
  // a Source Buffer.
  Monitor mWaitForDataMonitor;
};

NS_DEFINE_STATIC_IID_ACCESSOR(MediaSource, MOZILLA_DOM_MEDIASOURCE_IMPLEMENTATION_IID)

} // namespace dom

} // namespace mozilla
#endif /* mozilla_dom_MediaSource_h_ */
