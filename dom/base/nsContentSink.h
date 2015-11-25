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
#include "nsAutoPtr.h"
#include "nsGkAtoms.h"
#include "nsITimer.h"
#include "nsStubDocumentObserver.h"
#include "nsIContentSink.h"
#include "mozilla/Logging.h"
#include "nsCycleCollectionParticipant.h"
#include "nsThreadUtils.h"

class nsIDocument;
class nsIURI;
class nsIChannel;
class nsIDocShell;
class nsIAtom;
class nsIChannel;
class nsIContent;
class nsNodeInfoManager;
class nsScriptLoader;
class nsIApplicationCache;

namespace mozilla {
namespace css {
class Loader;
} // namespace css
} // namespace mozilla

#ifdef DEBUG

extern mozilla::LazyLogModule gContentSinkLogModuleInfo;

#define SINK_TRACE_CALLS              0x1
#define SINK_TRACE_REFLOW             0x2
#define SINK_ALWAYS_REFLOW            0x4

#define SINK_LOG_TEST(_lm, _bit) (int((_lm)->Level()) & (_bit))

#define SINK_TRACE(_lm, _bit, _args) \
  PR_BEGIN_MACRO                     \
    if (SINK_LOG_TEST(_lm, _bit)) {  \
      PR_LogPrint _args;             \
    }                                \
  PR_END_MACRO

#else
#define SINK_TRACE(_lm, _bit, _args)
#endif

#undef SINK_NO_INCREMENTAL

//----------------------------------------------------------------------

// 1/2 second fudge factor for window creation
#define NS_DELAY_FOR_WINDOW_CREATION  500000

class nsContentSink : public nsICSSLoaderObserver,
                      public nsSupportsWeakReference,
                      public nsStubDocumentObserver,
                      public nsITimerCallback
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsContentSink,
                                           nsICSSLoaderObserver)
    // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(mozilla::CSSStyleSheet* aSheet,
                              bool aWasAlternate,
                              nsresult aStatus) override;

  virtual nsresult ProcessMETATag(nsIContent* aContent);

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
  bool LinkContextIsOurDocument(const nsSubstring& aAnchor);
  bool Decode5987Format(nsAString& aEncoded);

  static void InitializeStatics();

protected:
  nsContentSink();
  virtual ~nsContentSink();

  enum CacheSelectionAction {
    // There is no offline cache manifest specified by the document,
    // or the document was loaded from a cache other than the one it
    // specifies via its manifest attribute and IS NOT a top-level
    // document, or an error occurred during the cache selection
    // algorithm.
    CACHE_SELECTION_NONE = 0,

    // The offline cache manifest must be updated.
    CACHE_SELECTION_UPDATE = 1,

    // The document was loaded from a cache other than the one it
    // specifies via its manifest attribute and IS a top-level
    // document.  In this case, the document is marked as foreign in
    // the cache it was loaded from and must be reloaded from the
    // correct cache (the one it specifies).
    CACHE_SELECTION_RELOAD = 2,

    // Some conditions require we must reselect the cache without the manifest
    CACHE_SELECTION_RESELECT_WITHOUT_MANIFEST = 3
  };

  nsresult Init(nsIDocument* aDoc, nsIURI* aURI,
                nsISupports* aContainer, nsIChannel* aChannel);

  nsresult ProcessHTTPHeaders(nsIChannel* aChannel);
  nsresult ProcessHeaderData(nsIAtom* aHeader, const nsAString& aValue,
                             nsIContent* aContent = nullptr);
  nsresult ProcessLinkHeader(const nsAString& aLinkData);
  nsresult ProcessLink(const nsSubstring& aAnchor,
                       const nsSubstring& aHref, const nsSubstring& aRel,
                       const nsSubstring& aTitle, const nsSubstring& aType,
                       const nsSubstring& aMedia, const nsSubstring& aCrossOrigin);

  virtual nsresult ProcessStyleLink(nsIContent* aElement,
                                    const nsSubstring& aHref,
                                    bool aAlternate,
                                    const nsSubstring& aTitle,
                                    const nsSubstring& aType,
                                    const nsSubstring& aMedia);

  void PrefetchHref(const nsAString &aHref, nsINode *aSource,
                    bool aExplicit);

  // For PrefetchDNS() aHref can either be the usual
  // URI format or of the form "//www.hostname.com" without a scheme.
  void PrefetchDNS(const nsAString &aHref);

  // Gets the cache key (used to identify items in a cache) of the channel.
  nsresult GetChannelCacheKey(nsIChannel* aChannel, nsACString& aCacheKey);

  // There is an offline cache manifest attribute specified and the
  // document is allowed to use the offline cache.  Process the cache
  // selection algorithm for this document and the manifest. Result is
  // an action that must be taken on the manifest, see
  // CacheSelectionAction enum above.
  //
  // @param aLoadApplicationCache
  //        The application cache from which the load originated, if
  //        any.
  // @param aManifestURI
  //        The manifest URI listed in the document.
  // @param aFetchedWithHTTPGetOrEquiv
  //        TRUE if this was fetched using the HTTP GET method.
  // @param aAction
  //        Out parameter, returns the action that should be performed
  //        by the calling function.
  nsresult SelectDocAppCache(nsIApplicationCache *aLoadApplicationCache,
                             nsIURI *aManifestURI,
                             bool aFetchedWithHTTPGetOrEquiv,
                             CacheSelectionAction *aAction);

  // There is no offline cache manifest attribute specified.  Process
  // the cache selection algorithm w/o the manifest. Result is an
  // action that must be taken, see CacheSelectionAction enum
  // above. In case the offline cache manifest has to be updated the
  // manifest URI is returned in aManifestURI.
  //
  // @param aLoadApplicationCache
  //        The application cache from which the load originated, if
  //        any.
  // @param aManifestURI
  //        Out parameter, returns the manifest URI of the cache that
  //        was selected.
  // @param aAction
  //        Out parameter, returns the action that should be performed
  //        by the calling function.
  nsresult SelectDocAppCacheNoManifest(nsIApplicationCache *aLoadApplicationCache,
                                       nsIURI **aManifestURI,
                                       CacheSelectionAction *aAction);

public:
  // Searches for the offline cache manifest attribute and calls one
  // of the above defined methods to select the document's application
  // cache, let it be associated with the document and eventually
  // schedule the cache update process.
  // This method MUST be called with the empty string as the argument
  // when there is no manifest attribute!
  void ProcessOfflineManifest(const nsAString& aManifestSpec);

  // Extracts the manifest attribute from the element if it is the root 
  // element and calls the above method.
  void ProcessOfflineManifest(nsIContent *aElement);

  // For Preconnect() aHref can either be the usual
  // URI format or of the form "//www.hostname.com" without a scheme.
  void Preconnect(const nsAString& aHref, const nsAString& aCrossOrigin);

protected:
  // Tries to scroll to the URI's named anchor. Once we've successfully
  // done that, further calls to this method will be ignored.
  void ScrollToRef();

  // Start layout.  If aIgnorePendingSheets is true, this will happen even if
  // we still have stylesheet loads pending.  Otherwise, we'll wait until the
  // stylesheets are all done loading.
public:
  void StartLayout(bool aIgnorePendingSheets);

  static void NotifyDocElementCreated(nsIDocument* aDoc);

protected:
  void
  FavorPerformanceHint(bool perfOverStarvation, uint32_t starvationDelay);

  inline int32_t GetNotificationInterval()
  {
    if (mDynamicLowerValue) {
      return 1000;
    }

    return sNotificationInterval;
  }

  virtual nsresult FlushTags() = 0;

  // Later on we might want to make this more involved somehow
  // (e.g. stop waiting after some timeout or whatnot).
  bool WaitForPendingSheets() { return mPendingSheetCount > 0; }

  void DoProcessLinkHeader();

  void StopDeflecting() {
    mDeflectedCount = sPerfDeflectCount;
  }

private:
  // People shouldn't be allocating this class directly.  All subclasses should
  // be allocated using a zeroing operator new.
  void* operator new(size_t sz) CPP_THROW_NEW;  // Not to be implemented

protected:

  nsCOMPtr<nsIDocument>         mDocument;
  RefPtr<nsParserBase>        mParser;
  nsCOMPtr<nsIURI>              mDocumentURI;
  nsCOMPtr<nsIDocShell>         mDocShell;
  RefPtr<mozilla::css::Loader> mCSSLoader;
  RefPtr<nsNodeInfoManager>   mNodeInfoManager;
  RefPtr<nsScriptLoader>      mScriptLoader;

  // back off timer notification after count
  int32_t mBackoffCount;

  // Time of last notification
  // Note: mLastNotificationTime is only valid once mLayoutStarted is true.
  PRTime mLastNotificationTime;

  // Timer used for notification
  nsCOMPtr<nsITimer> mNotificationTimer;

  // Have we already called BeginUpdate for this set of content changes?
  uint8_t mBeganUpdate : 1;
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

  // Do we notify based on time?
  static bool sNotifyOnTimer;
  // Back off timer notification after count.
  static int32_t sBackoffCount;
  // Notification interval in microseconds
  static int32_t sNotificationInterval;
  // How many times to deflect in interactive/perf modes
  static int32_t sInteractiveDeflectCount;
  static int32_t sPerfDeflectCount;
  // 0 = don't check for pending events
  // 1 = don't deflect if there are pending events
  // 2 = bail if there are pending events
  static int32_t sPendingEventMode;
  // How often to probe for pending events. 1=every token
  static int32_t sEventProbeRate;
  // How long to stay off the event loop in interactive/perf modes
  static int32_t sInteractiveParseTime;
  static int32_t sPerfParseTime;
  // How long to be in interactive mode after an event
  static int32_t sInteractiveTime;
  // How long to stay in perf mode after initial loading
  static int32_t sInitialPerfTime;
  // Should we switch between perf-mode and interactive-mode
  static int32_t sEnablePerfMode;
};

#endif // _nsContentSink_h_
