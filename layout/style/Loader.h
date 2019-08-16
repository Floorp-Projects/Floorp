/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* loading of CSS style sheets using the network APIs */

#ifndef mozilla_css_Loader_h
#define mozilla_css_Loader_h

#include "nsIPrincipal.h"
#include "nsCompatibility.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTObserverArray.h"
#include "nsURIHashKey.h"
#include "nsIStyleSheetLinkingElement.h"
#include "mozilla/Attributes.h"
#include "mozilla/CORSMode.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/net/ReferrerPolicy.h"

class nsICSSLoaderObserver;
class nsIConsoleReportCollector;
class nsIContent;

namespace mozilla {
namespace dom {
class DocGroup;
class Element;
}  // namespace dom
}  // namespace mozilla

namespace mozilla {

namespace css {

class SheetLoadData;
class ImportRule;

/*********************
 * Style sheet reuse *
 *********************/

class MOZ_RAII LoaderReusableStyleSheets {
 public:
  LoaderReusableStyleSheets() {}

  /**
   * Look for a reusable sheet (see AddReusableSheet) matching the
   * given URL.  If found, set aResult, remove the reused sheet from
   * the internal list, and return true.  If not found, return false;
   * in this case, aResult is not modified.
   *
   * @param aURL the url to match
   * @param aResult [out] the style sheet which can be reused
   */
  bool FindReusableStyleSheet(nsIURI* aURL, RefPtr<StyleSheet>& aResult);

  /**
   * Indicate that a certain style sheet is available for reuse if its
   * URI matches the URI of an @import.  Sheets should be added in the
   * opposite order in which they are intended to be reused.
   *
   * @param aSheet the sheet which can be reused
   */
  void AddReusableSheet(StyleSheet* aSheet) {
    mReusableSheets.AppendElement(aSheet);
  }

 private:
  LoaderReusableStyleSheets(const LoaderReusableStyleSheets&) = delete;
  LoaderReusableStyleSheets& operator=(const LoaderReusableStyleSheets&) =
      delete;

  // The sheets that can be reused.
  nsTArray<RefPtr<StyleSheet>> mReusableSheets;
};

class Loader final {
  typedef mozilla::net::ReferrerPolicy ReferrerPolicy;

 public:
  typedef nsIStyleSheetLinkingElement::Completed Completed;
  typedef nsIStyleSheetLinkingElement::HasAlternateRel HasAlternateRel;
  typedef nsIStyleSheetLinkingElement::IsAlternate IsAlternate;
  typedef nsIStyleSheetLinkingElement::IsInline IsInline;
  typedef nsIStyleSheetLinkingElement::IsExplicitlyEnabled IsExplicitlyEnabled;
  typedef nsIStyleSheetLinkingElement::MediaMatched MediaMatched;
  typedef nsIStyleSheetLinkingElement::Update LoadSheetResult;
  typedef nsIStyleSheetLinkingElement::SheetInfo SheetInfo;

  Loader();
  // aDocGroup is used for dispatching SheetLoadData in PostLoadEvent(). It
  // can be null if you want to use this constructor, and there's no
  // document when the Loader is constructed.
  explicit Loader(mozilla::dom::DocGroup*);
  explicit Loader(mozilla::dom::Document*);

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~Loader();

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Loader)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(Loader)

  void DropDocumentReference();  // notification that doc is going away

  void SetCompatibilityMode(nsCompatibility aCompatMode) {
    mCompatMode = aCompatMode;
  }
  nsCompatibility GetCompatibilityMode() { return mCompatMode; }

  // TODO(emilio): Is the complexity of this method and carrying the titles
  // around worth it? The alternate sheets will load anyhow eventually...
  void DocumentStyleSheetSetChanged();

  // XXXbz sort out what the deal is with events!  When should they fire?

  /**
   * Load an inline style sheet.  If a successful result is returned and
   * result.WillNotify() is true, then aObserver is guaranteed to be notified
   * asynchronously once the sheet is marked complete.  If an error is
   * returned, or if result.WillNotify() is false, aObserver will not be
   * notified.  In addition to parsing the sheet, this method will insert it
   * into the stylesheet list of this CSSLoader's document.
   * @param aObserver the observer to notify when the load completes.
   *        May be null.
   * @param aBuffer the stylesheet data
   * @param aLineNumber the line number at which the stylesheet data started.
   */
  Result<LoadSheetResult, nsresult> LoadInlineStyle(
      const SheetInfo&, const nsAString& aBuffer, uint32_t aLineNumber,
      nsICSSLoaderObserver* aObserver);

  /**
   * Load a linked (document) stylesheet.  If a successful result is returned,
   * aObserver is guaranteed to be notified asynchronously once the sheet is
   * loaded and marked complete, i.e., result.WillNotify() will always return
   * true.  If an error is returned, aObserver will not be notified.  In
   * addition to loading the sheet, this method will insert it into the
   * stylesheet list of this CSSLoader's document.
   * @param aObserver the observer to notify when the load completes.
   *                  May be null.
   */
  Result<LoadSheetResult, nsresult> LoadStyleLink(
      const SheetInfo&, nsICSSLoaderObserver* aObserver);

  /**
   * Load a child (@import-ed) style sheet.  In addition to loading the sheet,
   * this method will insert it into the child sheet list of aParentSheet.  If
   * there is no sheet currently being parsed and the child sheet is not
   * complete when this method returns, then when the child sheet becomes
   * complete aParentSheet will be QIed to nsICSSLoaderObserver and
   * asynchronously notified, just like for LoadStyleLink.  Note that if the
   * child sheet is already complete when this method returns, no
   * nsICSSLoaderObserver notification will be sent.
   *
   * @param aParentSheet the parent of this child sheet
   * @param aParentData the SheetLoadData corresponding to the load of the
   *                    parent sheet. May be null for @import rules inserted via
   *                    CSSOM.
   * @param aURL the URL of the child sheet
   * @param aMedia the already-parsed media list for the child sheet
   * @param aSavedSheets any saved style sheets which could be reused
   *              for this load
   */
  nsresult LoadChildSheet(StyleSheet& aParentSheet, SheetLoadData* aParentData,
                          nsIURI* aURL, dom::MediaList* aMedia,
                          LoaderReusableStyleSheets* aSavedSheets);

  enum class UseSystemPrincipal { No, Yes };

  /**
   * Synchronously load and return the stylesheet at aURL.  Any child sheets
   * will also be loaded synchronously.  Note that synchronous loads over some
   * protocols may involve spinning up a new event loop, so use of this method
   * does NOT guarantee not receiving any events before the sheet loads.  This
   * method can be used to load sheets not associated with a document.
   *
   * @param aURL the URL of the sheet to load
   * @param aParsingMode the mode in which to parse the sheet
   *        (see comments at enum SheetParsingMode, above).
   * @param aUseSystemPrincipal if true, give the resulting sheet the system
   * principal no matter where it's being loaded from.
   *
   * NOTE: At the moment, this method assumes the sheet will be UTF-8, but
   * ideally it would allow arbitrary encodings.  Callers should NOT depend on
   * non-UTF8 sheets being treated as UTF-8 by this method.
   *
   * NOTE: A successful return from this method doesn't indicate anything about
   * whether the data could be parsed as CSS and doesn't indicate anything
   * about the status of child sheets of the returned sheet.
   */
  Result<RefPtr<StyleSheet>, nsresult> LoadSheetSync(
      nsIURI*, SheetParsingMode = eAuthorSheetFeatures,
      UseSystemPrincipal = UseSystemPrincipal::No);

  enum class IsPreload { No, Yes };

  /**
   * Asynchronously load the stylesheet at aURL.  If a successful result is
   * returned, aObserver is guaranteed to be notified asynchronously once the
   * sheet is loaded and marked complete.  This method can be used to load
   * sheets not associated with a document.
   *
   * @param aURL the URL of the sheet to load
   * @param aParsingMode the mode in which to parse the sheet
   *        (see comments at enum SheetParsingMode, above).
   * @param aUseSystemPrincipal if true, give the resulting sheet the system
   * principal no matter where it's being loaded from.
   * @param aReferrerInfo referrer information of the sheet.
   * @param aObserver the observer to notify when the load completes.
   *                  Must not be null.
   * @return the sheet to load. Note that the sheet may well not be loaded by
   * the time this method returns.
   *
   * NOTE: At the moment, this method assumes the sheet will be UTF-8, but
   * ideally it would allow arbitrary encodings.  Callers should NOT depend on
   * non-UTF8 sheets being treated as UTF-8 by this method.
   */
  Result<RefPtr<StyleSheet>, nsresult> LoadSheet(
      nsIURI* aURI, IsPreload, nsIPrincipal* aOriginPrincipal,
      const Encoding* aPreloadEncoding, nsIReferrerInfo* aReferrerInfo,
      nsICSSLoaderObserver* aObserver, CORSMode aCORSMode = CORS_NONE,
      const nsAString& aIntegrity = EmptyString());

  /**
   * As above, but without caring for a couple things.
   */
  Result<RefPtr<StyleSheet>, nsresult> LoadSheet(nsIURI*, SheetParsingMode,
                                                 UseSystemPrincipal,
                                                 nsICSSLoaderObserver*);

  /**
   * Stop loading all sheets.  All nsICSSLoaderObservers involved will be
   * notified with NS_BINDING_ABORTED as the status, possibly synchronously.
   */
  void Stop();

  /**
   * nsresult Loader::StopLoadingSheet(nsIURI* aURL), which notifies the
   * nsICSSLoaderObserver with NS_BINDING_ABORTED, was removed in Bug 556446.
   * It can be found in revision 2c44a32052ad.
   */

  /**
   * Whether the loader is enabled or not.
   * When disabled, processing of new styles is disabled and an attempt
   * to do so will fail with a return code of
   * NS_ERROR_NOT_AVAILABLE. Note that this DOES NOT disable
   * currently loading styles or already processed styles.
   */
  bool GetEnabled() { return mEnabled; }
  void SetEnabled(bool aEnabled) { mEnabled = aEnabled; }

  /**
   * Get the document we live for. May return null.
   */
  mozilla::dom::Document* GetDocument() const { return mDocument; }

  /**
   * Return true if this loader has pending loads (ones that would send
   * notifications to an nsICSSLoaderObserver attached to this loader).
   * If called from inside nsICSSLoaderObserver::StyleSheetLoaded, this will
   * return false if and only if that is the last StyleSheetLoaded
   * notification the CSSLoader knows it's going to send.  In other words, if
   * two sheets load at once (via load coalescing, e.g.), HasPendingLoads()
   * will return true during notification for the first one, and false
   * during notification for the second one.
   */
  bool HasPendingLoads();

  /**
   * Add an observer to this loader.  The observer will be notified
   * for all loads that would have notified their own observers (even
   * if those loads don't have observers attached to them).
   * Load-specific observers will be notified before generic
   * observers.  The loader holds a reference to the observer.
   *
   * aObserver must not be null.
   */
  void AddObserver(nsICSSLoaderObserver* aObserver);

  /**
   * Remove an observer added via AddObserver.
   */
  void RemoveObserver(nsICSSLoaderObserver* aObserver);

  // These interfaces are public only for the benefit of static functions
  // within nsCSSLoader.cpp.

  // IsAlternateSheet can change our currently selected style set if none is
  // selected and aHasAlternateRel is false.
  IsAlternate IsAlternateSheet(const nsAString& aTitle, bool aHasAlternateRel);

  typedef nsTArray<RefPtr<SheetLoadData>> LoadDataArray;

  // Measure our size.
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  friend class SheetLoadData;
  friend class StreamLoader;

  // Helpers to conditionally block onload if mDocument is non-null.
  void BlockOnload();
  void UnblockOnload(bool aFireSync);

  // Helper to select the correct dispatch target for asynchronous events for
  // this loader.
  already_AddRefed<nsISerialEventTarget> DispatchTarget();

  nsresult CheckContentPolicy(nsIPrincipal* aLoadingPrincipal,
                              nsIPrincipal* aTriggeringPrincipal,
                              nsIURI* aTargetURI, nsINode* aRequestingNode,
                              IsPreload);

  enum class SheetState : uint8_t {
    Unknown = 0,
    NeedsParser,
    Pending,
    Loading,
    Complete
  };

  Tuple<RefPtr<StyleSheet>, SheetState> CreateSheet(
      const SheetInfo& aInfo, nsIPrincipal* aLoaderPrincipal,
      css::SheetParsingMode aParsingMode, bool aSyncLoad) {
    return CreateSheet(aInfo.mURI, aInfo.mContent, aLoaderPrincipal,
                       aParsingMode, aInfo.mCORSMode, aInfo.mReferrerInfo,
                       aInfo.mIntegrity, aSyncLoad);
  }

  // For inline style, the aURI param is null, but the aLinkingContent
  // must be non-null then.  The loader principal must never be null
  // if aURI is not null.
  Tuple<RefPtr<StyleSheet>, SheetState> CreateSheet(
      nsIURI* aURI, nsIContent* aLinkingContent, nsIPrincipal* aLoaderPrincipal,
      css::SheetParsingMode, CORSMode, nsIReferrerInfo* aLoadingReferrerInfo,
      const nsAString& aIntegrity, bool aSyncLoad);

  // Pass in either a media string or the MediaList from the CSSParser.  Don't
  // pass both.
  //
  // This method will set the sheet's enabled state based on IsAlternate and co.
  MediaMatched PrepareSheet(StyleSheet&, const nsAString& aTitle,
                            const nsAString& aMediaString, dom::MediaList*,
                            IsAlternate, IsExplicitlyEnabled);

  // Inserts a style sheet in a document or a ShadowRoot.
  void InsertSheetInTree(StyleSheet& aSheet, nsIContent* aLinkingContent);
  // Inserts a style sheet into a parent style sheet.
  void InsertChildSheet(StyleSheet& aSheet, StyleSheet& aParentSheet);

  Result<RefPtr<StyleSheet>, nsresult> InternalLoadNonDocumentSheet(
      nsIURI* aURL, IsPreload, SheetParsingMode aParsingMode,
      UseSystemPrincipal, nsIPrincipal* aOriginPrincipal,
      const Encoding* aPreloadEncoding, nsIReferrerInfo* aReferrerInfo,
      nsICSSLoaderObserver* aObserver, CORSMode aCORSMode,
      const nsAString& aIntegrity);

  // Post a load event for aObserver to be notified about aSheet.  The
  // notification will be sent with status NS_OK unless the load event is
  // canceled at some point (in which case it will be sent with
  // NS_BINDING_ABORTED).  aWasAlternate indicates the state when the load was
  // initiated, not the state at some later time.  aURI should be the URI the
  // sheet was loaded from (may be null for inline sheets).  aElement is the
  // owning element for this sheet.
  nsresult PostLoadEvent(nsIURI* aURI, StyleSheet* aSheet,
                         nsICSSLoaderObserver* aObserver,
                         IsAlternate aWasAlternate, MediaMatched aMediaMatched,
                         nsIReferrerInfo* aReferrerInfo,
                         nsIStyleSheetLinkingElement* aElement);

  // Start the loads of all the sheets in mPendingDatas
  void StartDeferredLoads();

  void HandleLoadEvent(SheetLoadData&);

  // Note: LoadSheet is responsible for setting the sheet to complete on
  // failure.
  nsresult LoadSheet(SheetLoadData&, SheetState, IsPreload);

  enum class AllowAsyncParse {
    Yes,
    No,
  };

  // Parse the stylesheet in the load data.
  //
  // Returns whether the parse finished. It may not finish e.g. if the sheet had
  // an @import.
  //
  // If this function returns Completed::Yes, then ParseSheet also called
  // SheetComplete on aLoadData.
  Completed ParseSheet(const nsACString&, SheetLoadData&, AllowAsyncParse);

  // The load of the sheet in the load data is done, one way or another.
  // Do final cleanup.
  void SheetComplete(SheetLoadData&, nsresult aStatus);

  // The guts of SheetComplete.  This may be called recursively on parent datas
  // or datas that had glommed on to a single load.  The array is there so load
  // datas whose observers need to be notified can be added to it.
  void DoSheetComplete(SheetLoadData&, LoadDataArray& aDatasToNotify);

  // Mark the given SheetLoadData, as well as any of its siblings, parents, etc
  // transitively, as failed.  The idea is to mark as failed any load that was
  // directly or indirectly @importing the sheet this SheetLoadData represents.
  void MarkLoadTreeFailed(SheetLoadData&);

  struct Sheets;
  UniquePtr<Sheets> mSheets;

  // The array of posted stylesheet loaded events (SheetLoadDatas) we have.
  // Note that these are rare.
  LoadDataArray mPostedEvents;

  // Our array of "global" observers
  nsTObserverArray<nsCOMPtr<nsICSSLoaderObserver>> mObservers;

  // This reference is nulled by the Document in it's destructor through
  // DropDocumentReference().
  mozilla::dom::Document* MOZ_NON_OWNING_REF
      mDocument;  // the document we live for

  // For dispatching events via DocGroup::Dispatch() when mDocument is nullptr.
  RefPtr<mozilla::dom::DocGroup> mDocGroup;

  // Number of datas still waiting to be notified on if we're notifying on a
  // whole bunch at once (e.g. in one of the stop methods).  This is used to
  // make sure that HasPendingLoads() won't return false until we're notifying
  // on the last data we're working with.
  uint32_t mDatasToNotifyOn;

  nsCompatibility mCompatMode;

  bool mEnabled;  // is enabled to load new styles

  nsCOMPtr<nsIConsoleReportCollector> mReporter;

#ifdef DEBUG
  // Whether we're in a necko callback atm.
  bool mSyncCallback = false;
#endif
};

}  // namespace css
}  // namespace mozilla

#endif /* mozilla_css_Loader_h */
