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
#include "mozilla/MozPromise.h"
#include "mozilla/dom/MediaSourceBinding.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"
#include "nsISupports.h"
#include "nscore.h"
#include "TimeUnits.h"

struct JSContext;
class JSObject;
class nsPIDOMWindowInner;

namespace mozilla {

class AbstractThread;
class ErrorResult;
template <typename T> class AsyncEventRunner;
class MediaResult;

namespace dom {
class MediaSource;
} // namespace dom
DDLoggedTypeName(dom::MediaSource);

namespace dom {

class GlobalObject;
class SourceBuffer;
class SourceBufferList;
template <typename T> class Optional;

#define MOZILLA_DOM_MEDIASOURCE_IMPLEMENTATION_IID \
  { 0x3839d699, 0x22c5, 0x439f, \
  { 0x94, 0xca, 0x0e, 0x0b, 0x26, 0xf9, 0xca, 0xbf } }

class MediaSource final
  : public DOMEventTargetHelper
  , public DecoderDoctorLifeLogger<MediaSource>
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
  void EndOfStream(const MediaResult& aError);

  void SetLiveSeekableRange(double aStart, double aEnd, ErrorResult& aRv);
  void ClearLiveSeekableRange(ErrorResult& aRv);

  static bool IsTypeSupported(const GlobalObject&, const nsAString& aType);
  static nsresult IsTypeSupported(const nsAString& aType, DecoderDoctorDiagnostics* aDiagnostics);

  static bool Enabled(JSContext* cx, JSObject* aGlobal);

  IMPL_EVENT_HANDLER(sourceopen);
  IMPL_EVENT_HANDLER(sourceended);
  IMPL_EVENT_HANDLER(sourceclosed);

  /** End WebIDL Methods. */

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaSource, DOMEventTargetHelper)
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_MEDIASOURCE_IMPLEMENTATION_IID)

  nsPIDOMWindowInner* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

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

  nsIPrincipal* GetPrincipal()
  {
    return mPrincipal;
  }

  // Returns a string describing the state of the MediaSource internal
  // buffered data. Used for debugging purposes.
  void GetMozDebugReaderData(nsAString& aString);

  bool HasLiveSeekableRange() const { return mLiveSeekableRange.isSome(); }
  media::TimeInterval LiveSeekableRange() const
  {
    return mLiveSeekableRange.value();
  }

  AbstractThread* AbstractMainThread() const
  {
    return mAbstractMainThread;
  }

  // Resolve all CompletionPromise pending.
  void CompletePendingTransactions();

private:
  // SourceBuffer uses SetDuration and SourceBufferIsActive
  friend class mozilla::dom::SourceBuffer;

  ~MediaSource();

  explicit MediaSource(nsPIDOMWindowInner* aWindow);

  friend class AsyncEventRunner<MediaSource>;
  void DispatchSimpleEvent(const char* aName);
  void QueueAsyncSimpleEvent(const char* aName);

  void DurationChange(double aNewDuration, ErrorResult& aRv);

  // SetDuration with no checks.
  void SetDuration(double aDuration);

  typedef MozPromise<bool, MediaResult, /* IsExclusive = */ true>
    ActiveCompletionPromise;
  // Mark SourceBuffer as active and rebuild ActiveSourceBuffers.
  // Return a MozPromise that will be resolved once all related operations are
  // completed, or can't progress any further.
  // Such as, transition of readyState from HAVE_NOTHING to HAVE_METADATA.
  RefPtr<ActiveCompletionPromise> SourceBufferIsActive(
    SourceBuffer* aSourceBuffer);

  RefPtr<SourceBufferList> mSourceBuffers;
  RefPtr<SourceBufferList> mActiveSourceBuffers;

  RefPtr<MediaSourceDecoder> mDecoder;
  // Ensures the media element remains alive to dispatch progress and
  // durationchanged events.
  RefPtr<HTMLMediaElement> mMediaElement;

  RefPtr<nsIPrincipal> mPrincipal;

  const RefPtr<AbstractThread> mAbstractMainThread;

  MediaSourceReadyState mReadyState;

  Maybe<media::TimeInterval> mLiveSeekableRange;
  nsTArray<MozPromiseHolder<ActiveCompletionPromise>> mCompletionPromises;
};

NS_DEFINE_STATIC_IID_ACCESSOR(MediaSource, MOZILLA_DOM_MEDIASOURCE_IMPLEMENTATION_IID)

} // namespace dom

} // namespace mozilla

#endif /* mozilla_dom_MediaSource_h_ */
