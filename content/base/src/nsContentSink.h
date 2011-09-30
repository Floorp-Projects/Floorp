/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Base class for the XML and HTML content sinks, which construct a
 * DOM based on information from the parser.
 */

#ifndef _nsContentSink_h_
#define _nsContentSink_h_

// Base class for contentsink implementations.

#include "nsICSSLoaderObserver.h"
#include "nsIScriptLoaderObserver.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsGkAtoms.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsStubDocumentObserver.h"
#include "nsIParserService.h"
#include "nsIContentSink.h"
#include "prlog.h"
#include "nsIRequest.h"
#include "nsCycleCollectionParticipant.h"
#include "nsThreadUtils.h"

class nsIDocument;
class nsIURI;
class nsIChannel;
class nsIDocShell;
class nsIParser;
class nsIAtom;
class nsIChannel;
class nsIContent;
class nsIViewManager;
class nsNodeInfoManager;
class nsScriptLoader;
class nsIApplicationCache;

namespace mozilla {
namespace css {
class Loader;
}
}

#ifdef NS_DEBUG

extern PRLogModuleInfo* gContentSinkLogModuleInfo;

#define SINK_TRACE_CALLS              0x1
#define SINK_TRACE_REFLOW             0x2
#define SINK_ALWAYS_REFLOW            0x4

#define SINK_LOG_TEST(_lm, _bit) (PRIntn((_lm)->level) & (_bit))

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
                      public nsIScriptLoaderObserver,
                      public nsSupportsWeakReference,
                      public nsStubDocumentObserver,
                      public nsITimerCallback
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsContentSink,
                                           nsIScriptLoaderObserver)
  NS_DECL_NSISCRIPTLOADEROBSERVER

    // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsCSSStyleSheet* aSheet, bool aWasAlternate,
                              nsresult aStatus);

  virtual nsresult ProcessMETATag(nsIContent* aContent);

  // nsIContentSink implementation helpers
  NS_HIDDEN_(nsresult) WillParseImpl(void);
  NS_HIDDEN_(nsresult) WillInterruptImpl(void);
  NS_HIDDEN_(nsresult) WillResumeImpl(void);
  NS_HIDDEN_(nsresult) DidProcessATokenImpl(void);
  NS_HIDDEN_(void) WillBuildModelImpl(void);
  NS_HIDDEN_(void) DidBuildModelImpl(bool aTerminated);
  NS_HIDDEN_(void) DropParserAndPerfHint(void);
  bool IsScriptExecutingImpl();

  void NotifyAppend(nsIContent* aContent, PRUint32 aStartIndex);

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER_BEGINUPDATE
  NS_DECL_NSIDOCUMENTOBSERVER_ENDUPDATE

  virtual void UpdateChildCounts() = 0;

  bool IsTimeToNotify();
  bool LinkContextIsOurDocument(const nsSubstring& aAnchor);

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
                             nsIContent* aContent = nsnull);
  nsresult ProcessLinkHeader(nsIContent* aElement,
                             const nsAString& aLinkData);
  nsresult ProcessLink(nsIContent* aElement, const nsSubstring& aAnchor,
                       const nsSubstring& aHref, const nsSubstring& aRel,
                       const nsSubstring& aTitle, const nsSubstring& aType,
                       const nsSubstring& aMedia);

  virtual nsresult ProcessStyleLink(nsIContent* aElement,
                                    const nsSubstring& aHref,
                                    bool aAlternate,
                                    const nsSubstring& aTitle,
                                    const nsSubstring& aType,
                                    const nsSubstring& aMedia);

  void PrefetchHref(const nsAString &aHref, nsIContent *aSource,
                    bool aExplicit);

  // aHref can either be the usual URI format or of the form "//www.hostname.com"
  // without a scheme.
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
  FavorPerformanceHint(bool perfOverStarvation, PRUint32 starvationDelay);

  inline PRInt32 GetNotificationInterval()
  {
    if (mDynamicLowerValue) {
      return 1000;
    }

    return sNotificationInterval;
  }

  // Overridable hooks into script evaluation
  virtual void PreEvaluateScript()                            {return;}
  virtual void PostEvaluateScript(nsIScriptElement *aElement) {return;}

  virtual nsresult FlushTags() = 0;

  // Later on we might want to make this more involved somehow
  // (e.g. stop waiting after some timeout or whatnot).
  bool WaitForPendingSheets() { return mPendingSheetCount > 0; }

  void DoProcessLinkHeader();

private:
  // People shouldn't be allocating this class directly.  All subclasses should
  // be allocated using a zeroing operator new.
  void* operator new(size_t sz) CPP_THROW_NEW;  // Not to be implemented

protected:

  virtual void ContinueInterruptedParsingAsync();
  void ContinueInterruptedParsingIfEnabled();

  nsCOMPtr<nsIDocument>         mDocument;
  nsCOMPtr<nsIParser>           mParser;
  nsCOMPtr<nsIURI>              mDocumentURI;
  nsCOMPtr<nsIDocShell>         mDocShell;
  nsRefPtr<mozilla::css::Loader> mCSSLoader;
  nsRefPtr<nsNodeInfoManager>   mNodeInfoManager;
  nsRefPtr<nsScriptLoader>      mScriptLoader;

  nsCOMArray<nsIScriptElement> mScriptElements;

  // back off timer notification after count
  PRInt32 mBackoffCount;

  // Time of last notification
  // Note: mLastNotificationTime is only valid once mLayoutStarted is true.
  PRTime mLastNotificationTime;

  // Timer used for notification
  nsCOMPtr<nsITimer> mNotificationTimer;

  // Have we already called BeginUpdate for this set of content changes?
  PRUint8 mBeganUpdate : 1;
  PRUint8 mLayoutStarted : 1;
  PRUint8 mCanInterruptParser : 1;
  PRUint8 mDynamicLowerValue : 1;
  PRUint8 mParsing : 1;
  PRUint8 mDroppedTimer : 1;
  // If true, we deferred starting layout until sheets load
  PRUint8 mDeferredLayoutStart : 1;
  // If true, we deferred notifications until sheets load
  PRUint8 mDeferredFlushTags : 1;
  // If false, we're not ourselves a document observer; that means we
  // shouldn't be performing any more content model notifications,
  // since we're not longer updating our child counts.
  PRUint8 mIsDocumentObserver : 1;
  // True if this is a fragment parser
  PRUint8 mFragmentMode : 1;
  // True to call prevent script execution in the fragment mode.
  PRUint8 mPreventScriptExecution : 1;
  
  //
  // -- Can interrupt parsing members --
  //

  // The number of tokens that have been processed since we measured
  // if it's time to return to the main event loop.
  PRUint32 mDeflectedCount;

  // Is there currently a pending event?
  bool mHasPendingEvent;

  // When to return to the main event loop
  PRUint32 mCurrentParseEndTime;

  PRInt32 mBeginLoadTime;

  // Last mouse event or keyboard event time sampled by the content
  // sink
  PRUint32 mLastSampledUserEventTime;

  PRInt32 mInMonolithicContainer;

  PRInt32 mInNotification;
  PRUint32 mUpdatesInNotification;

  PRUint32 mPendingSheetCount;

  nsRevocableEventPtr<nsRunnableMethod<nsContentSink, void, false> >
    mProcessLinkHeaderEvent;

  // Do we notify based on time?
  static bool sNotifyOnTimer;
  // Back off timer notification after count.
  static PRInt32 sBackoffCount;
  // Notification interval in microseconds
  static PRInt32 sNotificationInterval;
  // How many times to deflect in interactive/perf modes
  static PRInt32 sInteractiveDeflectCount;
  static PRInt32 sPerfDeflectCount;
  // 0 = don't check for pending events
  // 1 = don't deflect if there are pending events
  // 2 = bail if there are pending events
  static PRInt32 sPendingEventMode;
  // How often to probe for pending events. 1=every token
  static PRInt32 sEventProbeRate;
  // How long to stay off the event loop in interactive/perf modes
  static PRInt32 sInteractiveParseTime;
  static PRInt32 sPerfParseTime;
  // How long to be in interactive mode after an event
  static PRInt32 sInteractiveTime;
  // How long to stay in perf mode after initial loading
  static PRInt32 sInitialPerfTime;
  // Should we switch between perf-mode and interactive-mode
  static PRInt32 sEnablePerfMode;
  static bool sCanInterruptParser;
};

#endif // _nsContentSink_h_
