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
#include "nsTimer.h"
#include "nsCycleCollectionParticipant.h"

class nsIDocument;
class nsIURI;
class nsIChannel;
class nsIDocShell;
class nsICSSLoader;
class nsIParser;
class nsIAtom;
class nsIChannel;
class nsIContent;
class nsIViewManager;
class nsNodeInfoManager;
class nsScriptLoader;
class nsIApplicationCache;

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

// 200 determined empirically to provide good user response without
// sampling the clock too often.
#define NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE 200

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
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aWasAlternate,
                              nsresult aStatus);

  virtual nsresult ProcessMETATag(nsIContent* aContent);

  // nsIContentSink implementation helpers
  NS_HIDDEN_(nsresult) WillParseImpl(void);
  NS_HIDDEN_(nsresult) WillInterruptImpl(void);
  NS_HIDDEN_(nsresult) WillResumeImpl(void);
  NS_HIDDEN_(nsresult) DidProcessATokenImpl(void);
  NS_HIDDEN_(void) WillBuildModelImpl(void);
  NS_HIDDEN_(void) DidBuildModelImpl(void);
  NS_HIDDEN_(PRBool) ReadyToCallDidBuildModelImpl(void);
  NS_HIDDEN_(void) DropParserAndPerfHint(void);

  void NotifyAppend(nsIContent* aContent, PRUint32 aStartIndex);

  // nsIDocumentObserver
  virtual void BeginUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType);
  virtual void EndUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType);

  virtual void UpdateChildCounts() = 0;

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
  nsresult ProcessLink(nsIContent* aElement, const nsSubstring& aHref,
                       const nsSubstring& aRel, const nsSubstring& aTitle,
                       const nsSubstring& aType, const nsSubstring& aMedia);

  virtual nsresult ProcessStyleLink(nsIContent* aElement,
                                    const nsSubstring& aHref,
                                    PRBool aAlternate,
                                    const nsSubstring& aTitle,
                                    const nsSubstring& aType,
                                    const nsSubstring& aMedia);

  void PrefetchHref(const nsAString &aHref, nsIContent *aSource,
                    PRBool aExplicit);

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
                             PRBool aFetchedWithHTTPGetOrEquiv,
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

  // Searches for the offline cache manifest attribute and calls one
  // of the above defined methods to select the document's application
  // cache, let it be associated with the document and eventually
  // schedule the cache update process.
  void ProcessOfflineManifest(nsIContent *aElement);

  // Tries to scroll to the URI's named anchor. Once we've successfully
  // done that, further calls to this method will be ignored.
  void ScrollToRef();
  nsresult RefreshIfEnabled(nsIViewManager* vm);

  // Start layout.  If aIgnorePendingSheets is true, this will happen even if
  // we still have stylesheet loads pending.  Otherwise, we'll wait until the
  // stylesheets are all done loading.
  void StartLayout(PRBool aIgnorePendingSheets);

  PRBool IsTimeToNotify();

  void
  FavorPerformanceHint(PRBool perfOverStarvation, PRUint32 starvationDelay);

  inline PRInt32 GetNotificationInterval()
  {
    if (mDynamicLowerValue) {
      return 1000;
    }

    return mNotificationInterval;
  }

  inline PRInt32 GetMaxTokenProcessingTime()
  {
    if (mDynamicLowerValue) {
      return 3000;
    }

    return mMaxTokenProcessingTime;
  }

  // Overridable hooks into script evaluation
  virtual void PreEvaluateScript()                            {return;}
  virtual void PostEvaluateScript(nsIScriptElement *aElement) {return;}

  virtual nsresult FlushTags() = 0;

  // Later on we might want to make this more involved somehow
  // (e.g. stop waiting after some timeout or whatnot).
  PRBool WaitForPendingSheets() { return mPendingSheetCount > 0; }

private:
  // People shouldn't be allocating this class directly.  All subclasses should
  // be allocated using a zeroing operator new.
  void* operator new(size_t sz) CPP_THROW_NEW;  // Not to be implemented

protected:

  void ContinueInterruptedParsingAsyncIfEnabled();
  void ContinueInterruptedParsingIfEnabled();

  nsCOMPtr<nsIDocument>         mDocument;
  nsCOMPtr<nsIParser>           mParser;
  nsCOMPtr<nsIURI>              mDocumentURI;
  nsCOMPtr<nsIURI>              mDocumentBaseURI;
  nsCOMPtr<nsIDocShell>         mDocShell;
  nsCOMPtr<nsICSSLoader>        mCSSLoader;
  nsRefPtr<nsNodeInfoManager>   mNodeInfoManager;
  nsRefPtr<nsScriptLoader>      mScriptLoader;

  nsCOMArray<nsIScriptElement> mScriptElements;

  nsCString mRef; // ScrollTo #ref

  // back off timer notification after count
  PRInt32 mBackoffCount;

  // Notification interval in microseconds
  PRInt32 mNotificationInterval;

  // Time of last notification
  // Note: mLastNotificationTime is only valid once mLayoutStarted is true.
  PRTime mLastNotificationTime;

  // Timer used for notification
  nsCOMPtr<nsITimer> mNotificationTimer;

  // The number of tokens that have been processed while in the low
  // frequency parser interrupt mode without falling through to the
  // logic which decides whether to switch to the high frequency
  // parser interrupt mode.
  PRUint8 mDeflectedCount;

  // Do we notify based on time?
  PRPackedBool mNotifyOnTimer;

  // Have we already called BeginUpdate for this set of content changes?
  PRUint8 mBeganUpdate : 1;
  PRUint8 mLayoutStarted : 1;
  PRUint8 mScrolledToRefAlready : 1;
  PRUint8 mCanInterruptParser : 1;
  PRUint8 mDynamicLowerValue : 1;
  PRUint8 mParsing : 1;
  PRUint8 mDroppedTimer : 1;
  PRUint8 mChangeScrollPosWhenScrollingToRef : 1;
  // If true, we deferred starting layout until sheets load
  PRUint8 mDeferredLayoutStart : 1;
  // If true, we deferred notifications until sheets load
  PRUint8 mDeferredFlushTags : 1;
  PRUint8 mDidGetReadyToCallDidBuildModelCall : 1;
  
  // -- Can interrupt parsing members --
  PRUint32 mDelayTimerStart;

  // Interrupt parsing during token procesing after # of microseconds
  PRInt32 mMaxTokenProcessingTime;

  // Switch between intervals when time is exceeded
  PRInt32 mDynamicIntervalSwitchThreshold;

  PRInt32 mBeginLoadTime;

  // Last mouse event or keyboard event time sampled by the content
  // sink
  PRUint32 mLastSampledUserEventTime;

  PRInt32 mInMonolithicContainer;

  PRInt32 mInNotification;
  PRUint32 mUpdatesInNotification;

  PRUint32 mPendingSheetCount;

  // Measures content model creation time for current document
  MOZ_TIMER_DECLARE(mWatch)
};

// sanitizing content sink whitelists
extern PRBool IsAttrURI(nsIAtom *aName);
extern nsIAtom** const kDefaultAllowedTags [];
extern nsIAtom** const kDefaultAllowedAttributes [];

#endif // _nsContentSink_h_
