/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* loading of CSS style sheets using the network APIs */

#ifndef mozilla_css_Loader_h
#define mozilla_css_Loader_h

#include "nsIPrincipal.h"
#include "nsAString.h"
#include "nsAutoPtr.h"
#include "nsCompatibility.h"
#include "nsDataHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"
#include "nsTObserverArray.h"
#include "nsURIHashKey.h"
#include "mozilla/Attributes.h"
#include "mozilla/CORSMode.h"
#include "mozilla/MemoryReporting.h"

class nsIAtom;
class nsICSSLoaderObserver;
class nsCSSStyleSheet;
class nsIContent;
class nsIDocument;
class nsCSSParser;
class nsMediaList;
class nsIStyleSheetLinkingElement;
class nsCycleCollectionTraversalCallback;

namespace mozilla {
namespace dom {
class Element;
}
}

namespace mozilla {

class URIPrincipalAndCORSModeHashKey : public nsURIHashKey
{
public:
  typedef URIPrincipalAndCORSModeHashKey* KeyType;
  typedef const URIPrincipalAndCORSModeHashKey* KeyTypePointer;

  URIPrincipalAndCORSModeHashKey(const URIPrincipalAndCORSModeHashKey* aKey)
    : nsURIHashKey(aKey->mKey), mPrincipal(aKey->mPrincipal),
      mCORSMode(aKey->mCORSMode)
  {
    MOZ_COUNT_CTOR(URIPrincipalAndCORSModeHashKey);
  }
  URIPrincipalAndCORSModeHashKey(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                 CORSMode aCORSMode)
    : nsURIHashKey(aURI), mPrincipal(aPrincipal), mCORSMode(aCORSMode)
  {
    MOZ_COUNT_CTOR(URIPrincipalAndCORSModeHashKey);
  }
  URIPrincipalAndCORSModeHashKey(const URIPrincipalAndCORSModeHashKey& toCopy)
    : nsURIHashKey(toCopy), mPrincipal(toCopy.mPrincipal),
      mCORSMode(toCopy.mCORSMode)
  {
    MOZ_COUNT_CTOR(URIPrincipalAndCORSModeHashKey);
  }
  ~URIPrincipalAndCORSModeHashKey()
  {
    MOZ_COUNT_DTOR(URIPrincipalAndCORSModeHashKey);
  }

  URIPrincipalAndCORSModeHashKey* GetKey() const {
    return const_cast<URIPrincipalAndCORSModeHashKey*>(this);
  }
  const URIPrincipalAndCORSModeHashKey* GetKeyPointer() const { return this; }

  bool KeyEquals(const URIPrincipalAndCORSModeHashKey* aKey) const {
    if (!nsURIHashKey::KeyEquals(aKey->mKey)) {
      return false;
    }

    if (!mPrincipal != !aKey->mPrincipal) {
      // One or the other has a principal, but not both... not equal
      return false;
    }

    if (mCORSMode != aKey->mCORSMode) {
      // Different CORS modes; we don't match
      return false;
    }

    bool eq;
    return !mPrincipal ||
      (NS_SUCCEEDED(mPrincipal->Equals(aKey->mPrincipal, &eq)) && eq);
  }

  static const URIPrincipalAndCORSModeHashKey*
  KeyToPointer(URIPrincipalAndCORSModeHashKey* aKey) { return aKey; }
  static PLDHashNumber HashKey(const URIPrincipalAndCORSModeHashKey* aKey) {
    return nsURIHashKey::HashKey(aKey->mKey);
  }

  nsIURI* GetURI() const { return nsURIHashKey::GetKey(); }

  enum { ALLOW_MEMMOVE = true };

protected:
  nsCOMPtr<nsIPrincipal> mPrincipal;
  CORSMode mCORSMode;
};



namespace css {

class SheetLoadData;
class ImportRule;

/***********************************************************************
 * Enum that describes the state of the sheet returned by CreateSheet. *
 ***********************************************************************/
enum StyleSheetState {
  eSheetStateUnknown = 0,
  eSheetNeedsParser,
  eSheetPending,
  eSheetLoading,
  eSheetComplete
};

class Loader MOZ_FINAL {
public:
  Loader();
  Loader(nsIDocument*);
  ~Loader();

  NS_INLINE_DECL_REFCOUNTING(Loader)

  void DropDocumentReference(); // notification that doc is going away

  void SetCompatibilityMode(nsCompatibility aCompatMode)
  { mCompatMode = aCompatMode; }
  nsCompatibility GetCompatibilityMode() { return mCompatMode; }
  nsresult SetPreferredSheet(const nsAString& aTitle);

  // XXXbz sort out what the deal is with events!  When should they fire?

  /**
   * Load an inline style sheet.  If a successful result is returned and
   * *aCompleted is false, then aObserver is guaranteed to be notified
   * asynchronously once the sheet is marked complete.  If an error is
   * returned, or if *aCompleted is true, aObserver will not be notified.  In
   * addition to parsing the sheet, this method will insert it into the
   * stylesheet list of this CSSLoader's document.
   *
   * @param aElement the element linking to the stylesheet.  This must not be
   *                 null and must implement nsIStyleSheetLinkingElement.
   * @param aBuffer the stylesheet data
   * @param aLineNumber the line number at which the stylesheet data started.
   * @param aTitle the title of the sheet.
   * @param aMedia the media string for the sheet.
   * @param aObserver the observer to notify when the load completes.
   *        May be null.
   * @param [out] aCompleted whether parsing of the sheet completed.
   * @param [out] aIsAlternate whether the stylesheet ended up being an
   *        alternate sheet.
   */
  nsresult LoadInlineStyle(nsIContent* aElement,
                           const nsAString& aBuffer,
                           uint32_t aLineNumber,
                           const nsAString& aTitle,
                           const nsAString& aMedia,
                           mozilla::dom::Element* aScopeElement,
                           nsICSSLoaderObserver* aObserver,
                           bool* aCompleted,
                           bool* aIsAlternate);

  /**
   * Load a linked (document) stylesheet.  If a successful result is returned,
   * aObserver is guaranteed to be notified asynchronously once the sheet is
   * loaded and marked complete.  If an error is returned, aObserver will not
   * be notified.  In addition to loading the sheet, this method will insert it
   * into the stylesheet list of this CSSLoader's document.
   *
   * @param aElement the element linking to the the stylesheet.  May be null.
   * @param aURL the URL of the sheet.
   * @param aTitle the title of the sheet.
   * @param aMedia the media string for the sheet.
   * @param aHasAlternateRel whether the rel for this link included
   *        "alternate".
   * @param aCORSMode the CORS mode for this load.
   * @param aObserver the observer to notify when the load completes.
   *                  May be null.
   * @param [out] aIsAlternate whether the stylesheet actually ended up beinga
   *        an alternate sheet.  Note that this need not match
   *        aHasAlternateRel.
   */
  nsresult LoadStyleLink(nsIContent* aElement,
                         nsIURI* aURL,
                         const nsAString& aTitle,
                         const nsAString& aMedia,
                         bool aHasAlternateRel,
                         CORSMode aCORSMode,
                         nsICSSLoaderObserver* aObserver,
                         bool* aIsAlternate);

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
   * @param aURL the URL of the child sheet
   * @param aMedia the already-parsed media list for the child sheet
   * @param aRule the @import rule importing this child.  This is used to
   *              properly order the child sheet list of aParentSheet.
   */
  nsresult LoadChildSheet(nsCSSStyleSheet* aParentSheet,
                          nsIURI* aURL,
                          nsMediaList* aMedia,
                          ImportRule* aRule);

  /**
   * Synchronously load and return the stylesheet at aURL.  Any child sheets
   * will also be loaded synchronously.  Note that synchronous loads over some
   * protocols may involve spinning up a new event loop, so use of this method
   * does NOT guarantee not receiving any events before the sheet loads.  This
   * method can be used to load sheets not associated with a document.
   *
   * @param aURL the URL of the sheet to load
   * @param aEnableUnsafeRules whether unsafe rules are enabled for this
   * sheet load
   * Unsafe rules are rules that can violate key Gecko invariants if misused.
   * In particular, most anonymous box pseudoelements must be very carefully
   * styled or we will have severe problems. Therefore unsafe rules should
   * never be enabled for stylesheets controlled by untrusted sites; preferably
   * unsafe rules should only be enabled for agent sheets.
   * @param aUseSystemPrincipal if true, give the resulting sheet the system
   * principal no matter where it's being loaded from.
   * @param [out] aSheet the loaded, complete sheet.
   *
   * NOTE: At the moment, this method assumes the sheet will be UTF-8, but
   * ideally it would allow arbitrary encodings.  Callers should NOT depend on
   * non-UTF8 sheets being treated as UTF-8 by this method.
   *
   * NOTE: A successful return from this method doesn't indicate anything about
   * whether the data could be parsed as CSS and doesn't indicate anything
   * about the status of child sheets of the returned sheet.
   */
  nsresult LoadSheetSync(nsIURI* aURL, bool aEnableUnsafeRules,
                         bool aUseSystemPrincipal,
                         nsCSSStyleSheet** aSheet);

  /**
   * As above, but aUseSystemPrincipal and aEnableUnsafeRules are assumed false.
   */
  nsresult LoadSheetSync(nsIURI* aURL, nsCSSStyleSheet** aSheet) {
    return LoadSheetSync(aURL, false, false, aSheet);
  }

  /**
   * Asynchronously load the stylesheet at aURL.  If a successful result is
   * returned, aObserver is guaranteed to be notified asynchronously once the
   * sheet is loaded and marked complete.  This method can be used to load
   * sheets not associated with a document.
   *
   * @param aURL the URL of the sheet to load
   * @param aOriginPrincipal the principal to use for security checks.  This
   *                         can be null to indicate that these checks should
   *                         be skipped.
   * @param aCharset the encoding to use for converting the sheet data
   *        from bytes to Unicode.  May be empty to indicate that the
   *        charset of the CSSLoader's document should be used.  This
   *        is only used if neither the network transport nor the
   *        sheet itself indicate an encoding.
   * @param aObserver the observer to notify when the load completes.
   *                  Must not be null.
   * @param [out] aSheet the sheet to load. Note that the sheet may well
   *              not be loaded by the time this method returns.
   */
  nsresult LoadSheet(nsIURI* aURL,
                     nsIPrincipal* aOriginPrincipal,
                     const nsCString& aCharset,
                     nsICSSLoaderObserver* aObserver,
                     nsCSSStyleSheet** aSheet);

  /**
   * Same as above, to be used when the caller doesn't care about the
   * not-yet-loaded sheet.
   */
  nsresult LoadSheet(nsIURI* aURL,
                     nsIPrincipal* aOriginPrincipal,
                     const nsCString& aCharset,
                     nsICSSLoaderObserver* aObserver,
                     CORSMode aCORSMode = CORS_NONE);

  /**
   * Stop loading all sheets.  All nsICSSLoaderObservers involved will be
   * notified with NS_BINDING_ABORTED as the status, possibly synchronously.
   */
  nsresult Stop(void);

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
  nsIDocument* GetDocument() const { return mDocument; }

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
  nsresult AddObserver(nsICSSLoaderObserver* aObserver);

  /**
   * Remove an observer added via AddObserver.
   */
  void RemoveObserver(nsICSSLoaderObserver* aObserver);

  // These interfaces are public only for the benefit of static functions
  // within nsCSSLoader.cpp.

  // IsAlternate can change our currently selected style set if none
  // is selected and aHasAlternateRel is false.
  bool IsAlternate(const nsAString& aTitle, bool aHasAlternateRel);

  typedef nsTArray<nsRefPtr<SheetLoadData> > LoadDataArray;

  // Traverse the cached stylesheets we're holding on to.  This should
  // only be called from the document that owns this loader.
  void TraverseCachedSheets(nsCycleCollectionTraversalCallback& cb);

  // Unlink the cached stylesheets we're holding on to.  Again, this
  // should only be called from the document that owns this loader.
  void UnlinkCachedSheets();

  // Measure our size.
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Marks all the sheets at the given URI obsolete, and removes them from the
  // cache.
  nsresult ObsoleteSheet(nsIURI* aURI);

private:
  friend class SheetLoadData;

  static PLDHashOperator
  RemoveEntriesWithURI(URIPrincipalAndCORSModeHashKey* aKey,
                       nsRefPtr<nsCSSStyleSheet> &aSheet,
                       void* aUserData);

  // Note: null aSourcePrincipal indicates that the content policy and
  // CheckLoadURI checks should be skipped.
  nsresult CheckLoadAllowed(nsIPrincipal* aSourcePrincipal,
                            nsIURI* aTargetURI,
                            nsISupports* aContext);


  // For inline style, the aURI param is null, but the aLinkingContent
  // must be non-null then.  The loader principal must never be null
  // if aURI is not null.
  // *aIsAlternate is set based on aTitle and aHasAlternateRel.
  nsresult CreateSheet(nsIURI* aURI,
                       nsIContent* aLinkingContent,
                       nsIPrincipal* aLoaderPrincipal,
                       CORSMode aCORSMode,
                       bool aSyncLoad,
                       bool aHasAlternateRel,
                       const nsAString& aTitle,
                       StyleSheetState& aSheetState,
                       bool *aIsAlternate,
                       nsCSSStyleSheet** aSheet);

  // Pass in either a media string or the nsMediaList from the
  // CSSParser.  Don't pass both.
  // This method will set the sheet's enabled state based on isAlternate
  nsresult PrepareSheet(nsCSSStyleSheet* aSheet,
                        const nsAString& aTitle,
                        const nsAString& aMediaString,
                        nsMediaList* aMediaList,
                        mozilla::dom::Element* aScopeElement,
                        bool isAlternate);

  nsresult InsertSheetInDoc(nsCSSStyleSheet* aSheet,
                            nsIContent* aLinkingContent,
                            nsIDocument* aDocument);

  nsresult InsertChildSheet(nsCSSStyleSheet* aSheet,
                            nsCSSStyleSheet* aParentSheet,
                            ImportRule* aParentRule);

  nsresult InternalLoadNonDocumentSheet(nsIURI* aURL,
                                        bool aAllowUnsafeRules,
                                        bool aUseSystemPrincipal,
                                        nsIPrincipal* aOriginPrincipal,
                                        const nsCString& aCharset,
                                        nsCSSStyleSheet** aSheet,
                                        nsICSSLoaderObserver* aObserver,
                                        CORSMode aCORSMode = CORS_NONE);

  // Post a load event for aObserver to be notified about aSheet.  The
  // notification will be sent with status NS_OK unless the load event is
  // canceled at some point (in which case it will be sent with
  // NS_BINDING_ABORTED).  aWasAlternate indicates the state when the load was
  // initiated, not the state at some later time.  aURI should be the URI the
  // sheet was loaded from (may be null for inline sheets).  aElement is the
  // owning element for this sheet.
  nsresult PostLoadEvent(nsIURI* aURI,
                         nsCSSStyleSheet* aSheet,
                         nsICSSLoaderObserver* aObserver,
                         bool aWasAlternate,
                         nsIStyleSheetLinkingElement* aElement);

  // Start the loads of all the sheets in mPendingDatas
  void StartAlternateLoads();

  // Handle an event posted by PostLoadEvent
  void HandleLoadEvent(SheetLoadData* aEvent);

  // Note: LoadSheet is responsible for releasing aLoadData and setting the
  // sheet to complete on failure.
  nsresult LoadSheet(SheetLoadData* aLoadData, StyleSheetState aSheetState);

  // Parse the stylesheet in aLoadData.  The sheet data comes from aInput.
  // Set aCompleted to true if the parse finished, false otherwise (e.g. if the
  // sheet had an @import).  If aCompleted is true when this returns, then
  // ParseSheet also called SheetComplete on aLoadData.
  nsresult ParseSheet(const nsAString& aInput,
                      SheetLoadData* aLoadData,
                      bool& aCompleted);

  // The load of the sheet in aLoadData is done, one way or another.  Do final
  // cleanup, including releasing aLoadData.
  void SheetComplete(SheetLoadData* aLoadData, nsresult aStatus);

  // The guts of SheetComplete.  This may be called recursively on parent datas
  // or datas that had glommed on to a single load.  The array is there so load
  // datas whose observers need to be notified can be added to it.
  void DoSheetComplete(SheetLoadData* aLoadData, nsresult aStatus,
                       LoadDataArray& aDatasToNotify);

  nsRefPtrHashtable<URIPrincipalAndCORSModeHashKey, nsCSSStyleSheet>
                    mCompleteSheets;
  nsDataHashtable<URIPrincipalAndCORSModeHashKey, SheetLoadData*>
                    mLoadingDatas; // weak refs
  nsDataHashtable<URIPrincipalAndCORSModeHashKey, SheetLoadData*>
                    mPendingDatas; // weak refs

  // We're not likely to have many levels of @import...  But likely to have
  // some.  Allocate some storage, what the hell.
  nsAutoTArray<SheetLoadData*, 8> mParsingDatas;

  // The array of posted stylesheet loaded events (SheetLoadDatas) we have.
  // Note that these are rare.
  LoadDataArray     mPostedEvents;

  // Our array of "global" observers
  // XXXbz these are strong refs; should we be cycle collecting CSS loaders?
  nsTObserverArray<nsCOMPtr<nsICSSLoaderObserver> > mObservers;

  // the load data needs access to the document...
  nsIDocument*      mDocument;  // the document we live for


  // Number of datas still waiting to be notified on if we're notifying on a
  // whole bunch at once (e.g. in one of the stop methods).  This is used to
  // make sure that HasPendingLoads() won't return false until we're notifying
  // on the last data we're working with.
  uint32_t          mDatasToNotifyOn;

  nsCompatibility   mCompatMode;
  nsString          mPreferredSheet;  // title of preferred sheet

  bool              mEnabled; // is enabled to load new styles

#ifdef DEBUG
  bool              mSyncCallback;
#endif
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_Loader_h */
