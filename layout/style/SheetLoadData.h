/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_css_SheetLoadData_h
#define mozilla_css_SheetLoadData_h

#include "mozilla/css/Loader.h"
#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/Encoding.h"
#include "mozilla/PreloaderBase.h"
#include "mozilla/NotNull.h"
#include "mozilla/UniquePtr.h"
#include "nsIThreadInternal.h"
#include "nsProxyRelease.h"

namespace mozilla {
class StyleSheet;
}
class nsICSSLoaderObserver;
class nsINode;
class nsIPrincipal;
class nsIURI;
class nsIReferrerInfo;
struct StyleUseCounters;

namespace mozilla {
namespace css {

/*********************************************
 * Data needed to properly load a stylesheet *
 *********************************************/

static_assert(eAuthorSheetFeatures == 0 && eUserSheetFeatures == 1 &&
                  eAgentSheetFeatures == 2,
              "sheet parsing mode constants won't fit "
              "in SheetLoadData::mParsingMode");

class SheetLoadData final : public PreloaderBase,
                            public nsIRunnable,
                            public nsIThreadObserver {
  using MediaMatched = dom::LinkStyle::MediaMatched;
  using IsAlternate = dom::LinkStyle::IsAlternate;
  using IsPreload = Loader::IsPreload;
  using UseSystemPrincipal = Loader::UseSystemPrincipal;

 protected:
  virtual ~SheetLoadData();

 public:
  // PreloaderBase
  static void PrioritizeAsPreload(nsIChannel* aChannel);
  void PrioritizeAsPreload() final;

  // Data for loading a sheet linked from a document
  SheetLoadData(Loader* aLoader, const nsAString& aTitle, nsIURI* aURI,
                StyleSheet* aSheet, bool aSyncLoad, nsINode* aOwningNode,
                IsAlternate aIsAlternate, MediaMatched aMediaMatched,
                IsPreload aIsPreload, nsICSSLoaderObserver* aObserver,
                nsIPrincipal* aTriggeringPrincipal,
                nsIReferrerInfo* aReferrerInfo, nsINode* aRequestingNode);

  // Data for loading a sheet linked from an @import rule
  SheetLoadData(Loader* aLoader, nsIURI* aURI, StyleSheet* aSheet,
                SheetLoadData* aParentData, nsICSSLoaderObserver* aObserver,
                nsIPrincipal* aTriggeringPrincipal,
                nsIReferrerInfo* aReferrerInfo, nsINode* aRequestingNode);

  // Data for loading a non-document sheet
  SheetLoadData(Loader* aLoader, nsIURI* aURI, StyleSheet* aSheet,
                bool aSyncLoad, UseSystemPrincipal, IsPreload,
                const Encoding* aPreloadEncoding,
                nsICSSLoaderObserver* aObserver,
                nsIPrincipal* aTriggeringPrincipal,
                nsIReferrerInfo* aReferrerInfo, nsINode* aRequestingNode);

  nsIReferrerInfo* ReferrerInfo() const { return mReferrerInfo; }

  void ScheduleLoadEventIfNeeded();

  NotNull<const Encoding*> DetermineNonBOMEncoding(const nsACString& aSegment,
                                                   nsIChannel*) const;

  // The caller may have the bytes for the stylesheet split across two strings,
  // so aBytes1 and aBytes2 refer to those pieces.
  nsresult VerifySheetReadyToParse(nsresult aStatus, const nsACString& aBytes1,
                                   const nsACString& aBytes2,
                                   nsIChannel* aChannel);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSITHREADOBSERVER

  // Hold a ref to the CSSLoader so we can call back to it to let it
  // know the load finished
  const RefPtr<Loader> mLoader;

  // Title needed to pull datas out of the pending datas table when
  // the preferred title is changed
  const nsString mTitle;

  // The encoding we decided to use for the sheet
  const Encoding* mEncoding;

  // URI we're loading.  Null for inline or constructable sheets.
  nsCOMPtr<nsIURI> mURI;

  // Should be 1 for non-inline sheets.
  uint32_t mLineNumber;

  // The sheet we're loading data for
  const RefPtr<StyleSheet> mSheet;

  // Linked list of datas for the same URI as us.
  RefPtr<SheetLoadData> mNext;

  // Load data for the sheet that @import-ed us if we were @import-ed
  // during the parse
  const RefPtr<SheetLoadData> mParentData;

  // The expiration time of the channel that has loaded this data, if
  // applicable.
  uint32_t mExpirationTime = 0;

  // Number of sheets we @import-ed that are still loading
  uint32_t mPendingChildren;

  // mSyncLoad is true when the load needs to be synchronous.
  // For LoadSheetSync, <link> to chrome stylesheets in UA Widgets,
  // and children of sync loads.
  const bool mSyncLoad : 1;

  // mIsNonDocumentSheet is true if the load was triggered by LoadSheetSync or
  // LoadSheet or an @import from such a sheet.  Non-document sheet loads can
  // proceed even if we have no document.
  const bool mIsNonDocumentSheet : 1;

  // mIsLoading is true from the moment we are placed in the loader's
  // "loading datas" table (right after the async channel is opened)
  // to the moment we are removed from said table (due to the load
  // completing or being cancelled).
  bool mIsLoading : 1;

  // mIsBeingParsed is true if this stylesheet is currently being parsed.
  bool mIsBeingParsed : 1;

  // mIsCancelled is set to true when a sheet load is stopped by
  // Stop() or StopLoadingSheet() (which was removed in Bug 556446).
  // SheetLoadData::OnStreamComplete() checks this to avoid parsing
  // sheets that have been cancelled and such.
  bool mIsCancelled : 1;

  // mMustNotify is true if the load data is being loaded async and
  // the original function call that started the load has returned.
  // This applies only to observer notifications; load/error events
  // are fired for any SheetLoadData that has a non-null
  // mOwningElement.
  bool mMustNotify : 1;

  // mWasAlternate is true if the sheet was an alternate when the load data was
  // created.
  const bool mWasAlternate : 1;

  // mMediaMatched is true if the sheet matched its medialist when the load data
  // was created.
  const bool mMediaMatched : 1;

  // mUseSystemPrincipal is true if the system principal should be used for
  // this sheet, no matter what the channel principal is.  Only true for sync
  // loads.
  const bool mUseSystemPrincipal : 1;

  // If true, this SheetLoadData is being used as a way to handle
  // async observer notification for an already-complete sheet.
  bool mSheetAlreadyComplete : 1;

  // If true, the sheet is being loaded cross-origin without CORS permissions.
  // This is completely normal and CORS isn't needed for such loads.  This
  // flag is simply useful in determining whether to set mBlockResourceTiming
  // for a child sheet.
  bool mIsCrossOriginNoCORS : 1;

  // If this flag is true, LoadSheet will call SetReportResourceTiming(false)
  // on the timedChannel. This is to mark resources that are loaded by a
  // cross-origin stylesheet with a no-cors policy.
  // https://www.w3.org/TR/resource-timing/#processing-model
  bool mBlockResourceTiming : 1;

  // Boolean flag indicating whether the load has failed.  This will be set
  // to true if this load, or the load of any descendant import, fails.
  bool mLoadFailed : 1;

  // Whether this is a preload, and which kind of preload it is.
  //
  // TODO(emilio): This can become a bitfield once we build with a GCC version
  // that has the fix for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61414,
  // which causes a false positive warning here.
  const IsPreload mIsPreload;

  // This is the node that imported the sheet. Needed to get the charset set on
  // it, and to fire load/error events. Must implement LinkStyle.
  const nsCOMPtr<nsINode> mOwningNode;

  // The observer that wishes to be notified of load completion
  const nsCOMPtr<nsICSSLoaderObserver> mObserver;

  // The principal that identifies who started loading us.
  const nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;

  // Referrer info of the load.
  const nsCOMPtr<nsIReferrerInfo> mReferrerInfo;

  // The node that identifies who started loading us.
  const nsCOMPtr<nsINode> mRequestingNode;

  // The encoding guessed from attributes and the document character set.
  const NotNull<const Encoding*> mGuessedEncoding;

  // If we've parsed the stylesheet, the use counters for the properties parsed
  // in this styleshetet.
  UniquePtr<StyleUseCounters> mUseCounters;

  // The quirks mode of the loader at the time the load was triggered.
  const nsCompatibility mCompatMode;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  // Whether SheetComplete was called.
  bool mSheetCompleteCalled = false;
  // Whether we intentionally are not calling SheetComplete because nobody is
  // listening for the load.
  bool mIntentionallyDropped = false;
#endif

  bool ShouldDefer() const { return mWasAlternate || !mMediaMatched; }

  // If there are no child sheets outstanding, mark us as complete.
  // Otherwise, the children are holding strong refs to the data
  // and will call SheetComplete() on it when they complete.
  void SheetFinishedParsingAsync() {
    MOZ_ASSERT(mIsBeingParsed);
    mIsBeingParsed = false;
    if (!mPendingChildren) {
      mLoader->SheetComplete(*this, NS_OK);
    }
  }

  bool IsLinkPreload() const { return mIsPreload == IsPreload::FromLink; }

  bool BlocksLoadEvent() const { return !RootLoadData().IsLinkPreload(); }

 private:
  const SheetLoadData& RootLoadData() const {
    auto* top = this;
    while (top->mParentData) {
      top = top->mParentData;
    }
    return *top;
  }

  void FireLoadEvent(nsIThreadInternal* aThread);
};

using SheetLoadDataHolder = nsMainThreadPtrHolder<SheetLoadData>;

}  // namespace css
}  // namespace mozilla

/**
 * Casting SheetLoadData to nsISupports is ambiguous.
 * This method handles that.
 */
inline nsISupports* ToSupports(mozilla::css::SheetLoadData* p) {
  return NS_ISUPPORTS_CAST(nsIRunnable*, p);
}

#endif  // mozilla_css_SheetLoadData_h
