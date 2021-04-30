/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for the XML and HTML content sinks, which construct a
 * DOM based on information from the parser.
 */

#ifndef _nsContentSink_h_
#define _nsContentSink_h_

// Base class for contentsink implementations.

#include "mozilla/Attributes.h"
#include "nsICSSLoaderObserver.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsGkAtoms.h"
#include "nsITimer.h"
#include "nsStubDocumentObserver.h"
#include "nsIContentSink.h"
#include "mozilla/Logging.h"
#include "nsCycleCollectionParticipant.h"
#include "nsThreadUtils.h"
#include "mozilla/StaticPrefs_content.h"

class nsIURI;
class nsIChannel;
class nsIDocShell;
class nsAtom;
class nsIChannel;
class nsIContent;
class nsNodeInfoManager;

namespace mozilla {
namespace css {
class Loader;
}  // namespace css

namespace dom {
class Document;
class ScriptLoader;
}  // namespace dom
}  // namespace mozilla

#ifdef DEBUG

extern mozilla::LazyLogModule gContentSinkLogModuleInfo;

#  define SINK_TRACE_CALLS 0x1
#  define SINK_TRACE_REFLOW 0x2
#  define SINK_ALWAYS_REFLOW 0x4

#  define SINK_LOG_TEST(_lm, _bit) (int((_lm)->Level()) & (_bit))

#  define SINK_TRACE(_lm, _bit, _args) \
    do {                               \
      if (SINK_LOG_TEST(_lm, _bit)) {  \
        printf_stderr _args;           \
      }                                \
    } while (0)

#else
#  define SINK_TRACE(_lm, _bit, _args)
#endif

#undef SINK_NO_INCREMENTAL

//----------------------------------------------------------------------

class nsContentSink : public nsICSSLoaderObserver,
                      public nsSupportsWeakReference,
                      public nsStubDocumentObserver,
                      public nsITimerCallback,
                      public nsINamed {
 protected:
  typedef mozilla::dom::Document Document;

 private:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsContentSink, nsICSSLoaderObserver)
  // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK

  NS_DECL_NSINAMED

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(mozilla::StyleSheet* aSheet, bool aWasDeferred,
                              nsresult aStatus) override;

  // nsIContentSink implementation helpers
  nsresult WillParseImpl(void);
  nsresult WillInterruptImpl(void);
  nsresult WillResumeImpl(void);
  nsresult DidProcessATokenImpl(void);
  void WillBuildModelImpl(void);
  void DidBuildModelImpl(bool aTerminated);
  void DropParserAndPerfHint(void);
  bool IsScriptExecutingImpl();

  void NotifyAppend(nsIContent* aContent, uint32_t aStartIndex);

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER_BEGINUPDATE
  NS_DECL_NSIDOCUMENTOBSERVER_ENDUPDATE

  virtual void UpdateChildCounts() = 0;

  bool IsTimeToNotify();
  bool LinkContextIsOurDocument(const nsAString& aAnchor);
  bool Decode5987Format(nsAString& aEncoded);

 protected:
  nsContentSink();
  virtual ~nsContentSink();

  nsresult Init(Document* aDoc, nsIURI* aURI, nsISupports* aContainer,
                nsIChannel* aChannel);

  nsresult ProcessHTTPHeaders(nsIChannel* aChannel);
  nsresult ProcessLinkHeader(const nsAString& aLinkData);
  nsresult ProcessLinkFromHeader(
      const nsAString& aAnchor, const nsAString& aHref, const nsAString& aRel,
      const nsAString& aTitle, const nsAString& aIntegrity,
      const nsAString& aSrcset, const nsAString& aSizes, const nsAString& aType,
      const nsAString& aMedia, const nsAString& aCrossOrigin,
      const nsAString& aReferrerPolicy, const nsAString& aAs);

  virtual nsresult ProcessStyleLinkFromHeader(
      const nsAString& aHref, bool aAlternate, const nsAString& aTitle,
      const nsAString& aIntegrity, const nsAString& aType,
      const nsAString& aMedia, const nsAString& aReferrerPolicy);

  void PrefetchHref(const nsAString& aHref, const nsAString& aAs,
                    const nsAString& aType, const nsAString& aMedia);
  void PreloadHref(const nsAString& aHref, const nsAString& aAs,
                   const nsAString& aType, const nsAString& aMedia,
                   const nsAString& aIntegrity, const nsAString& aSrcset,
                   const nsAString& aSizes, const nsAString& aCORS,
                   const nsAString& aReferrerPolicy);

  // For PrefetchDNS() aHref can either be the usual
  // URI format or of the form "//www.hostname.com" without a scheme.
  void PrefetchDNS(const nsAString& aHref);

  // Gets the cache key (used to identify items in a cache) of the channel.
  nsresult GetChannelCacheKey(nsIChannel* aChannel, nsACString& aCacheKey);

 public:
  // For Preconnect() aHref can either be the usual
  // URI format or of the form "//www.hostname.com" without a scheme.
  void Preconnect(const nsAString& aHref, const nsAString& aCrossOrigin);

 protected:
  // Tries to scroll to the URI's named anchor. Once we've successfully
  // done that, further calls to this method will be ignored.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void ScrollToRef();

  // Start layout.  If aIgnorePendingSheets is true, this will happen even if
  // we still have stylesheet loads pending.  Otherwise, we'll wait until the
  // stylesheets are all done loading.
 public:
  void StartLayout(bool aIgnorePendingSheets);

  static void NotifyDocElementCreated(Document* aDoc);

  Document* GetDocument() { return mDocument; }

 protected:
  void FavorPerformanceHint(bool perfOverStarvation, uint32_t starvationDelay);

  inline int32_t GetNotificationInterval() {
    if (mDynamicLowerValue) {
      return 1000;
    }

    return mozilla::StaticPrefs::content_notify_interval();
  }

  virtual nsresult FlushTags() = 0;

  // Later on we might want to make this more involved somehow
  // (e.g. stop waiting after some timeout or whatnot).
  bool WaitForPendingSheets() { return mPendingSheetCount > 0; }

  void DoProcessLinkHeader();

  void StopDeflecting() {
    mDeflectedCount = mozilla::StaticPrefs::content_sink_perf_deflect_count();
  }

 protected:
  RefPtr<Document> mDocument;
  RefPtr<nsParserBase> mParser;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIDocShell> mDocShell;
  RefPtr<mozilla::css::Loader> mCSSLoader;
  RefPtr<nsNodeInfoManager> mNodeInfoManager;
  RefPtr<mozilla::dom::ScriptLoader> mScriptLoader;

  // back off timer notification after count
  int32_t mBackoffCount;

  // Time of last notification
  // Note: mLastNotificationTime is only valid once mLayoutStarted is true.
  PRTime mLastNotificationTime;

  // Timer used for notification
  nsCOMPtr<nsITimer> mNotificationTimer;

  uint8_t mLayoutStarted : 1;
  uint8_t mDynamicLowerValue : 1;
  uint8_t mParsing : 1;
  uint8_t mDroppedTimer : 1;
  // If true, we deferred starting layout until sheets load
  uint8_t mDeferredLayoutStart : 1;
  // If true, we deferred notifications until sheets load
  uint8_t mDeferredFlushTags : 1;
  // If false, we're not ourselves a document observer; that means we
  // shouldn't be performing any more content model notifications,
  // since we're not longer updating our child counts.
  uint8_t mIsDocumentObserver : 1;
  // True if this is parser is a fragment parser or an HTML DOMParser.
  // XML DOMParser leaves this to false for now!
  uint8_t mRunsToCompletion : 1;
  // True if we are blocking load event.
  bool mIsBlockingOnload : 1;

  //
  // -- Can interrupt parsing members --
  //

  // The number of tokens that have been processed since we measured
  // if it's time to return to the main event loop.
  uint32_t mDeflectedCount;

  // Is there currently a pending event?
  bool mHasPendingEvent;

  // When to return to the main event loop
  uint32_t mCurrentParseEndTime;

  int32_t mBeginLoadTime;

  // Last mouse event or keyboard event time sampled by the content
  // sink
  uint32_t mLastSampledUserEventTime;

  int32_t mInMonolithicContainer;

  int32_t mInNotification;
  uint32_t mUpdatesInNotification;

  uint32_t mPendingSheetCount;

  nsRevocableEventPtr<nsRunnableMethod<nsContentSink, void, false> >
      mProcessLinkHeaderEvent;
};

#endif  // _nsContentSink_h_
