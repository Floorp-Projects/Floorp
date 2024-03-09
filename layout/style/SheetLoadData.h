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
#include "mozilla/SharedSubResourceCache.h"
#include "mozilla/NotNull.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace dom {
enum class FetchPriority : uint8_t;
}  // namespace dom
class AsyncEventDispatcher;
class StyleSheet;
}  // namespace mozilla
class nsICSSLoaderObserver;
class nsINode;
class nsIPrincipal;
class nsIURI;
class nsIReferrerInfo;

namespace mozilla::css {

/*********************************************
 * Data needed to properly load a stylesheet *
 *********************************************/

static_assert(eAuthorSheetFeatures == 0 && eUserSheetFeatures == 1 &&
                  eAgentSheetFeatures == 2,
              "sheet parsing mode constants won't fit "
              "in SheetLoadData::mParsingMode");

enum class SyncLoad : bool { No, Yes };

class SheetLoadData final
    : public PreloaderBase,
      public SharedSubResourceCacheLoadingValueBase<SheetLoadData> {
  using MediaMatched = dom::LinkStyle::MediaMatched;
  using IsAlternate = dom::LinkStyle::IsAlternate;
  using UseSystemPrincipal = css::Loader::UseSystemPrincipal;

 protected:
  virtual ~SheetLoadData();

 public:
  static void PrioritizeAsPreload(nsIChannel* aChannel);

  // If this is a deferred load, start it now.
  void StartPendingLoad();

  // Data for loading a sheet linked from a document
  SheetLoadData(css::Loader*, const nsAString& aTitle, nsIURI*, StyleSheet*,
                SyncLoad, nsINode* aOwningNode, IsAlternate, MediaMatched,
                StylePreloadKind, nsICSSLoaderObserver* aObserver,
                nsIPrincipal* aTriggeringPrincipal, nsIReferrerInfo*,
                const nsAString& aNonce, dom::FetchPriority aFetchPriority);

  // Data for loading a sheet linked from an @import rule
  SheetLoadData(css::Loader*, nsIURI*, StyleSheet*, SheetLoadData* aParentData,
                nsICSSLoaderObserver* aObserver,
                nsIPrincipal* aTriggeringPrincipal, nsIReferrerInfo*);

  // Data for loading a non-document sheet
  SheetLoadData(css::Loader*, nsIURI*, StyleSheet*, SyncLoad,
                UseSystemPrincipal, StylePreloadKind,
                const Encoding* aPreloadEncoding,
                nsICSSLoaderObserver* aObserver,
                nsIPrincipal* aTriggeringPrincipal, nsIReferrerInfo*,
                const nsAString& aNonce, dom::FetchPriority aFetchPriority);

  nsIReferrerInfo* ReferrerInfo() const { return mReferrerInfo; }

  const nsString& Nonce() const { return mNonce; }

  already_AddRefed<AsyncEventDispatcher> PrepareLoadEventIfNeeded();

  NotNull<const Encoding*> DetermineNonBOMEncoding(const nsACString& aSegment,
                                                   nsIChannel*) const;

  // The caller may have the bytes for the stylesheet split across two strings,
  // so aBytes1 and aBytes2 refer to those pieces.
  nsresult VerifySheetReadyToParse(nsresult aStatus, const nsACString& aBytes1,
                                   const nsACString& aBytes2,
                                   nsIChannel* aChannel);

  NS_DECL_ISUPPORTS

  css::Loader& Loader() { return *mLoader; }

  void DidCancelLoad() { mIsCancelled = true; }

  // Hold a ref to the CSSLoader so we can call back to it to let it
  // know the load finished
  const RefPtr<css::Loader> mLoader;

  // Title needed to pull datas out of the pending datas table when
  // the preferred title is changed
  const nsString mTitle;

  // The encoding we decided to use for the sheet
  const Encoding* mEncoding;

  // URI we're loading.  Null for inline or constructable sheets.
  nsCOMPtr<nsIURI> mURI;

  // The sheet we're loading data for
  const RefPtr<StyleSheet> mSheet;

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

  // Whether this stylesheet is for a child sheet load. This is necessary
  // because the sheet could be detached mid-load by CSSOM.
  const bool mIsChildSheet : 1;

  // mIsBeingParsed is true if this stylesheet is currently being parsed.
  bool mIsBeingParsed : 1;

  // mIsLoading is set to true when a sheet load is initiated. This field is
  // also used by the SharedSubResourceCache to avoid having multiple loads for
  // the same resource.
  bool mIsLoading : 1;

  // mIsCancelled is set to true when a sheet load is stopped by
  // Stop() or StopLoadingSheet() (which was removed in Bug 556446).
  // SheetLoadData::OnStreamComplete() checks this to avoid parsing
  // sheets that have been cancelled and such.
  bool mIsCancelled : 1;

  // mMustNotify is true if the load data is being loaded async and the original
  // function call that started the load has returned.
  //
  // This applies only to observer notifications; load/error events are fired
  // for any SheetLoadData that has a non-null owner node (though mMustNotify is
  // used to avoid an event loop round-trip in that case).
  bool mMustNotify : 1;

  // Whether we had an owner node at the point of creation. This allows
  // differentiating between "Link" header stylesheets and LinkStyle-owned
  // stylesheets.
  const bool mHadOwnerNode : 1;

  // mWasAlternate is true if the sheet was an alternate
  // (https://html.spec.whatwg.org/#rel-alternate) when the load data was
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
  const StylePreloadKind mPreloadKind;

  nsINode* GetRequestingNode() const;

  // The observer that wishes to be notified of load completion
  nsCOMPtr<nsICSSLoaderObserver> mObserver;

  // The principal that identifies who started loading us.
  const nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;

  // Referrer info of the load.
  const nsCOMPtr<nsIReferrerInfo> mReferrerInfo;

  // The cryptographic nonce of the load used for CSP checks.
  const nsString mNonce;

  const dom::FetchPriority mFetchPriority;

  // The encoding guessed from attributes and the document character set.
  const NotNull<const Encoding*> mGuessedEncoding;

  // The quirks mode of the loader at the time the load was triggered.
  const nsCompatibility mCompatMode;

  // Whether SheetComplete was called.
  bool mSheetCompleteCalled = false;

  // Whether we intentionally are not calling SheetComplete because nobody is
  // listening for the load.
  bool mIntentionallyDropped = false;

  bool ShouldDefer() const { return mWasAlternate || !mMediaMatched; }

  RefPtr<StyleSheet> ValueForCache() const;
  uint32_t ExpirationTime() const { return mExpirationTime; }

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

  bool IsPreload() const { return mPreloadKind != StylePreloadKind::None; }
  bool IsLinkRelPreload() const { return css::IsLinkRelPreload(mPreloadKind); }

  bool BlocksLoadEvent() const {
    const auto& root = RootLoadData();
    return !root.IsLinkRelPreload() && !root.IsSyncLoad();
  }

  bool IsSyncLoad() const override { return mSyncLoad; }
  bool IsLoading() const override { return mIsLoading; }
  bool IsCancelled() const override { return mIsCancelled; }

  void StartLoading() override { mIsLoading = true; }
  void SetLoadCompleted() override { mIsLoading = false; }
  void Cancel() override { mIsCancelled = true; }

 private:
  const SheetLoadData& RootLoadData() const {
    const auto* top = this;
    while (top->mParentData) {
      top = top->mParentData;
    }
    return *top;
  }
};

using SheetLoadDataHolder = nsMainThreadPtrHolder<SheetLoadData>;

}  // namespace mozilla::css

#endif  // mozilla_css_SheetLoadData_h
