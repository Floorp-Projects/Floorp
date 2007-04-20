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
#include "nsITimer.h"
#include "nsStubDocumentObserver.h"
#include "nsIParserService.h"
#include "nsIContentSink.h"
#include "prlog.h"
#include "nsIRequest.h"
#include "nsTimer.h"

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
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTLOADEROBSERVER

    // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aWasAlternate,
                              nsresult aStatus);

  nsresult ProcessMETATag(nsIContent* aContent);

  // nsIContentSink implementation helpers
  NS_HIDDEN_(nsresult) WillInterruptImpl(void);
  NS_HIDDEN_(nsresult) WillResumeImpl(void);
  NS_HIDDEN_(nsresult) DidProcessATokenImpl(void);
  NS_HIDDEN_(void) WillBuildModelImpl(void);
  NS_HIDDEN_(void) DidBuildModelImpl(void);
  NS_HIDDEN_(void) DropParserAndPerfHint(void);
  NS_HIDDEN_(nsresult) WillProcessTokensImpl(void);

  void NotifyAppend(nsIContent* aContent, PRUint32 aStartIndex);

  // nsIDocumentObserver
  virtual void BeginUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType);
  virtual void EndUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType);

  virtual void UpdateChildCounts() = 0;

protected:
  nsContentSink();
  virtual ~nsContentSink();

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

  void PrefetchHref(const nsAString &aHref, PRBool aExplicit, PRBool aOffline);

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

  void TryToScrollToRef();

private:
  // People shouldn't be allocating this class directly.  All subclasses should
  // be allocated using a zeroing operator new.
  void* operator new(size_t sz) CPP_THROW_NEW;  // Not to be implemented

protected:

  void ContinueInterruptedParsingAsync();
  void ContinueInterruptedParsing();

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
  PRUint8 mInTitle : 1;
  PRUint8 mChangeScrollPosWhenScrollingToRef : 1;
  // If true, we deferred starting layout until sheets load
  PRUint8 mDeferredLayoutStart : 1;
  
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

  PRUint32 mPendingSheetCount;

  // Measures content model creation time for current document
  MOZ_TIMER_DECLARE(mWatch)
};


//
// these two lists are used by the sanitizing fragment serializers
// Thanks to Mark Pilgrim and Sam Ruby for the initial whitelist
//
static nsIAtom** const kDefaultAllowedTags [] = {
  &nsGkAtoms::a,
  &nsGkAtoms::abbr,
  &nsGkAtoms::acronym,
  &nsGkAtoms::address,
  &nsGkAtoms::area,
  &nsGkAtoms::b,
  &nsGkAtoms::bdo,
  &nsGkAtoms::big,
  &nsGkAtoms::blockquote,
  &nsGkAtoms::br,
  &nsGkAtoms::button,
  &nsGkAtoms::caption,
  &nsGkAtoms::center,
  &nsGkAtoms::cite,
  &nsGkAtoms::code,
  &nsGkAtoms::col,
  &nsGkAtoms::colgroup,
  &nsGkAtoms::dd,
  &nsGkAtoms::del,
  &nsGkAtoms::dfn,
  &nsGkAtoms::dir,
  &nsGkAtoms::div,
  &nsGkAtoms::dl,
  &nsGkAtoms::dt,
  &nsGkAtoms::em,
  &nsGkAtoms::fieldset,
  &nsGkAtoms::font,
  &nsGkAtoms::form,
  &nsGkAtoms::h1,
  &nsGkAtoms::h2,
  &nsGkAtoms::h3,
  &nsGkAtoms::h4,
  &nsGkAtoms::h5,
  &nsGkAtoms::h6,
  &nsGkAtoms::hr,
  &nsGkAtoms::i,
  &nsGkAtoms::img,
  &nsGkAtoms::input,
  &nsGkAtoms::ins,
  &nsGkAtoms::kbd,
  &nsGkAtoms::label,
  &nsGkAtoms::legend,
  &nsGkAtoms::li,
  &nsGkAtoms::listing,
  &nsGkAtoms::map,
  &nsGkAtoms::menu,
  &nsGkAtoms::nobr,
  &nsGkAtoms::ol,
  &nsGkAtoms::optgroup,
  &nsGkAtoms::option,
  &nsGkAtoms::p,
  &nsGkAtoms::pre,
  &nsGkAtoms::q,
  &nsGkAtoms::s,
  &nsGkAtoms::samp,
  &nsGkAtoms::select,
  &nsGkAtoms::small,
  &nsGkAtoms::span,
  &nsGkAtoms::strike,
  &nsGkAtoms::strong,
  &nsGkAtoms::sub,
  &nsGkAtoms::sup,
  &nsGkAtoms::table,
  &nsGkAtoms::tbody,
  &nsGkAtoms::td,
  &nsGkAtoms::textarea,
  &nsGkAtoms::tfoot,
  &nsGkAtoms::th,
  &nsGkAtoms::thead,
  &nsGkAtoms::tr,
  &nsGkAtoms::tt,
  &nsGkAtoms::u,
  &nsGkAtoms::ul,
  &nsGkAtoms::var
};

static nsIAtom** const kDefaultAllowedAttributes [] = {
  &nsGkAtoms::abbr,
  &nsGkAtoms::accept,
  &nsGkAtoms::acceptcharset,
  &nsGkAtoms::accesskey,
  &nsGkAtoms::action,
  &nsGkAtoms::align,
  &nsGkAtoms::alt,
  &nsGkAtoms::autocomplete,
  &nsGkAtoms::axis,
  &nsGkAtoms::background,
  &nsGkAtoms::bgcolor,
  &nsGkAtoms::border,
  &nsGkAtoms::cellpadding,
  &nsGkAtoms::cellspacing,
  &nsGkAtoms::_char,
  &nsGkAtoms::charoff,
  &nsGkAtoms::charset,
  &nsGkAtoms::checked,
  &nsGkAtoms::cite,
  &nsGkAtoms::_class,
  &nsGkAtoms::clear,
  &nsGkAtoms::cols,
  &nsGkAtoms::colspan,
  &nsGkAtoms::color,
  &nsGkAtoms::compact,
  &nsGkAtoms::coords,
  &nsGkAtoms::datetime,
  &nsGkAtoms::dir,
  &nsGkAtoms::disabled,
  &nsGkAtoms::enctype,
  &nsGkAtoms::_for,
  &nsGkAtoms::frame,
  &nsGkAtoms::headers,
  &nsGkAtoms::height,
  &nsGkAtoms::href,
  &nsGkAtoms::hreflang,
  &nsGkAtoms::hspace,
  &nsGkAtoms::id,
  &nsGkAtoms::ismap,
  &nsGkAtoms::label,
  &nsGkAtoms::lang,
  &nsGkAtoms::longdesc,
  &nsGkAtoms::maxlength,
  &nsGkAtoms::media,
  &nsGkAtoms::method,
  &nsGkAtoms::multiple,
  &nsGkAtoms::name,
  &nsGkAtoms::nohref,
  &nsGkAtoms::noshade,
  &nsGkAtoms::nowrap,
  &nsGkAtoms::pointSize,
  &nsGkAtoms::prompt,
  &nsGkAtoms::readonly,
  &nsGkAtoms::rel,
  &nsGkAtoms::rev,
  &nsGkAtoms::role,
  &nsGkAtoms::rows,
  &nsGkAtoms::rowspan,
  &nsGkAtoms::rules,
  &nsGkAtoms::scope,
  &nsGkAtoms::selected,
  &nsGkAtoms::shape,
  &nsGkAtoms::size,
  &nsGkAtoms::span,
  &nsGkAtoms::src,
  &nsGkAtoms::start,
  &nsGkAtoms::summary,
  &nsGkAtoms::tabindex,
  &nsGkAtoms::target,
  &nsGkAtoms::title,
  &nsGkAtoms::type,
  &nsGkAtoms::usemap,
  &nsGkAtoms::valign,
  &nsGkAtoms::value,
  &nsGkAtoms::vspace,
  &nsGkAtoms::width
};

// URIs action, href, src, longdesc, usemap, cite
static
PRBool IsAttrURI(nsIAtom *aName)
{
  return (aName == nsGkAtoms::action ||
          aName == nsGkAtoms::href ||
          aName == nsGkAtoms::src ||
          aName == nsGkAtoms::longdesc ||
          aName == nsGkAtoms::usemap ||
          aName == nsGkAtoms::cite ||
          aName == nsGkAtoms::background);
}
#endif // _nsContentSink_h_
