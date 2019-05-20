/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontFaceSet.h"

#include "gfxFontConstants.h"
#include "gfxFontSrcPrincipal.h"
#include "gfxFontSrcURI.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/CSSFontFaceRule.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FontFaceSetBinding.h"
#include "mozilla/dom/FontFaceSetIterator.h"
#include "mozilla/dom/FontFaceSetLoadEvent.h"
#include "mozilla/dom/FontFaceSetLoadEventBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/Telemetry.h"
#include "mozilla/LoadInfo.h"
#include "nsAutoPtr.h"
#include "nsContentPolicyUtils.h"
#include "nsDeviceContext.h"
#include "nsFontFaceLoader.h"
#include "nsIClassOfService.h"
#include "nsIConsoleService.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocShell.h"
#include "mozilla/dom/Document.h"
#include "nsILoadContext.h"
#include "nsINetworkPredictor.h"
#include "nsIPrincipal.h"
#include "nsISupportsPriority.h"
#include "nsIWebNavigation.h"
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"
#include "nsIInputStream.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"
#include "nsUTF8Utils.h"
#include "nsDOMNavigationTiming.h"
#include "ReferrerInfo.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

#define LOG(args) \
  MOZ_LOG(gfxUserFontSet::GetUserFontsLog(), mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(gfxUserFontSet::GetUserFontsLog(), LogLevel::Debug)

#define FONT_LOADING_API_ENABLED_PREF "layout.css.font-loading-api.enabled"

NS_IMPL_CYCLE_COLLECTION_CLASS(FontFaceSet)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FontFaceSet,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReady);
  for (size_t i = 0; i < tmp->mRuleFaces.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRuleFaces[i].mFontFace);
  }
  for (size_t i = 0; i < tmp->mNonRuleFaces.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNonRuleFaces[i].mFontFace);
  }
  if (tmp->mUserFontSet) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUserFontSet->mFontFaceSet);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FontFaceSet,
                                                DOMEventTargetHelper)
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReady);
  for (size_t i = 0; i < tmp->mRuleFaces.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mRuleFaces[i].mFontFace);
  }
  for (size_t i = 0; i < tmp->mNonRuleFaces.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mNonRuleFaces[i].mFontFace);
  }
  if (tmp->mUserFontSet) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mUserFontSet->mFontFaceSet);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mUserFontSet);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(FontFaceSet, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FontFaceSet, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FontFaceSet)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

FontFaceSet::FontFaceSet(nsPIDOMWindowInner* aWindow, dom::Document* aDocument)
    : DOMEventTargetHelper(aWindow),
      mDocument(aDocument),
      mStandardFontLoadPrincipal(
          new gfxFontSrcPrincipal(mDocument->NodePrincipal())),
      mResolveLazilyCreatedReadyPromise(false),
      mStatus(FontFaceSetLoadStatus::Loaded),
      mNonRuleFacesDirty(false),
      mHasLoadingFontFaces(false),
      mHasLoadingFontFacesIsDirty(false),
      mDelayedLoadCheck(false),
      mBypassCache(false),
      mPrivateBrowsing(false) {
  MOZ_ASSERT(mDocument, "We should get a valid document from the caller!");

  mStandardFontLoadPrincipal =
      new gfxFontSrcPrincipal(mDocument->NodePrincipal());

  // Record the state of the "bypass cache" flags from the docshell now,
  // since we want to look at them from style worker threads, and we can
  // only get to the docshell through a weak pointer (which is only
  // possible on the main thread).
  //
  // In theory the load type of a docshell could change after the document
  // is loaded, but handling that doesn't seem too important.
  if (nsCOMPtr<nsIDocShell> docShell = mDocument->GetDocShell()) {
    uint32_t loadType;
    uint32_t flags;
    if ((NS_SUCCEEDED(docShell->GetLoadType(&loadType)) &&
         ((loadType >> 16) & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE)) ||
        (NS_SUCCEEDED(docShell->GetDefaultLoadFlags(&flags)) &&
         (flags & nsIRequest::LOAD_BYPASS_CACHE))) {
      mBypassCache = true;
    }
  }

  // Same for the "private browsing" flag.
  if (nsCOMPtr<nsILoadContext> loadContext = mDocument->GetLoadContext()) {
    mPrivateBrowsing = loadContext->UsePrivateBrowsing();
  }

  if (!mDocument->DidFireDOMContentLoaded()) {
    mDocument->AddSystemEventListener(NS_LITERAL_STRING("DOMContentLoaded"),
                                      this, false, false);
  } else {
    // In some cases we can't rely on CheckLoadingFinished being called from
    // the refresh driver.  For example, documents in display:none iframes.
    // Or if the document has finished loading and painting at the time that
    // script requests document.fonts and causes us to get here.
    CheckLoadingFinished();
  }

  mDocument->CSSLoader()->AddObserver(this);

  mUserFontSet = new UserFontSet(this);
}

FontFaceSet::~FontFaceSet() {
  // Assert that we don't drop any FontFaceSet objects during a Servo traversal,
  // since PostTraversalTask objects can hold raw pointers to FontFaceSets.
  MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());

  Disconnect();
}

JSObject* FontFaceSet::WrapObject(JSContext* aContext,
                                  JS::Handle<JSObject*> aGivenProto) {
  return FontFaceSet_Binding::Wrap(aContext, this, aGivenProto);
}

void FontFaceSet::Disconnect() {
  RemoveDOMContentLoadedListener();

  if (mDocument && mDocument->CSSLoader()) {
    // We're null checking CSSLoader() since FontFaceSet::Disconnect() might be
    // being called during unlink, at which time the loader amy already have
    // been unlinked from the document.
    mDocument->CSSLoader()->RemoveObserver(this);
  }

  for (auto it = mLoaders.Iter(); !it.Done(); it.Next()) {
    it.Get()->GetKey()->Cancel();
  }

  mLoaders.Clear();
}

void FontFaceSet::RemoveDOMContentLoadedListener() {
  if (mDocument) {
    mDocument->RemoveSystemEventListener(NS_LITERAL_STRING("DOMContentLoaded"),
                                         this, false);
  }
}

void FontFaceSet::ParseFontShorthandForMatching(
    const nsAString& aFont, RefPtr<SharedFontList>& aFamilyList,
    FontWeight& aWeight, FontStretch& aStretch, FontSlantStyle& aStyle,
    ErrorResult& aRv) {
  StyleComputedFontStyleDescriptor style;
  float stretch;
  float weight;

  RefPtr<URLExtraData> url = ServoCSSParser::GetURLExtraData(mDocument);
  if (!ServoCSSParser::ParseFontShorthandForMatching(aFont, url, aFamilyList,
                                                     style, stretch, weight)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  switch (style.tag) {
    case StyleComputedFontStyleDescriptor::Tag::Normal:
      aStyle = FontSlantStyle::Normal();
      break;
    case StyleComputedFontStyleDescriptor::Tag::Italic:
      aStyle = FontSlantStyle::Italic();
      break;
    case StyleComputedFontStyleDescriptor::Tag::Oblique:
      MOZ_ASSERT(style.AsOblique()._0 == style.AsOblique()._1,
                 "We use ComputedFontStyleDescriptor just for convenience, "
                 "the two values should always match");
      aStyle = FontSlantStyle::Oblique(style.AsOblique()._0);
      break;
  }

  aWeight = FontWeight(weight);
  aStretch = FontStretch::FromStyle(stretch);
}

static bool HasAnyCharacterInUnicodeRange(gfxUserFontEntry* aEntry,
                                          const nsAString& aInput) {
  const char16_t* p = aInput.Data();
  const char16_t* end = p + aInput.Length();

  while (p < end) {
    uint32_t c = UTF16CharEnumerator::NextChar(&p, end);
    if (aEntry->CharacterInUnicodeRange(c)) {
      return true;
    }
  }
  return false;
}

void FontFaceSet::FindMatchingFontFaces(const nsAString& aFont,
                                        const nsAString& aText,
                                        nsTArray<FontFace*>& aFontFaces,
                                        ErrorResult& aRv) {
  RefPtr<SharedFontList> familyList;
  FontWeight weight;
  FontStretch stretch;
  FontSlantStyle italicStyle;
  ParseFontShorthandForMatching(aFont, familyList, weight, stretch, italicStyle,
                                aRv);
  if (aRv.Failed()) {
    return;
  }

  gfxFontStyle style;
  style.style = italicStyle;
  style.weight = weight;
  style.stretch = stretch;

  nsTArray<FontFaceRecord>* arrays[2];
  arrays[0] = &mNonRuleFaces;
  arrays[1] = &mRuleFaces;

  // Set of FontFaces that we want to return.
  nsTHashtable<nsPtrHashKey<FontFace>> matchingFaces;

  for (const FontFamilyName& fontFamilyName : familyList->mNames) {
    if (!fontFamilyName.IsNamed()) {
      continue;
    }

    RefPtr<gfxFontFamily> family =
        mUserFontSet->LookupFamily(nsAtomCString(fontFamilyName.mName));

    if (!family) {
      continue;
    }

    AutoTArray<gfxFontEntry*, 4> entries;
    family->FindAllFontsForStyle(style, entries);

    for (gfxFontEntry* e : entries) {
      FontFace::Entry* entry = static_cast<FontFace::Entry*>(e);
      if (HasAnyCharacterInUnicodeRange(entry, aText)) {
        for (FontFace* f : entry->GetFontFaces()) {
          matchingFaces.PutEntry(f);
        }
      }
    }
  }

  // Add all FontFaces in matchingFaces to aFontFaces, in the order
  // they appear in the FontFaceSet.
  for (nsTArray<FontFaceRecord>* array : arrays) {
    for (FontFaceRecord& record : *array) {
      FontFace* f = record.mFontFace;
      if (matchingFaces.Contains(f)) {
        aFontFaces.AppendElement(f);
      }
    }
  }
}

TimeStamp FontFaceSet::GetNavigationStartTimeStamp() {
  TimeStamp navStart;
  RefPtr<nsDOMNavigationTiming> timing(mDocument->GetNavigationTiming());
  if (timing) {
    navStart = timing->GetNavigationStartTimeStamp();
  }
  return navStart;
}

already_AddRefed<Promise> FontFaceSet::Load(JSContext* aCx,
                                            const nsAString& aFont,
                                            const nsAString& aText,
                                            ErrorResult& aRv) {
  FlushUserFontSet();

  nsTArray<RefPtr<Promise>> promises;

  nsTArray<FontFace*> faces;
  FindMatchingFontFaces(aFont, aText, faces, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  for (FontFace* f : faces) {
    RefPtr<Promise> promise = f->Load(aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    if (!promises.AppendElement(promise, fallible)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  }

  return Promise::All(aCx, promises, aRv);
}

bool FontFaceSet::Check(const nsAString& aFont, const nsAString& aText,
                        ErrorResult& aRv) {
  FlushUserFontSet();

  nsTArray<FontFace*> faces;
  FindMatchingFontFaces(aFont, aText, faces, aRv);
  if (aRv.Failed()) {
    return false;
  }

  for (FontFace* f : faces) {
    if (f->Status() != FontFaceLoadStatus::Loaded) {
      return false;
    }
  }

  return true;
}

bool FontFaceSet::ReadyPromiseIsPending() const {
  return mReady ? mReady->State() == Promise::PromiseState::Pending
                : !mResolveLazilyCreatedReadyPromise;
}

Promise* FontFaceSet::GetReady(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  // There may be outstanding style changes that will trigger the loading of
  // new fonts.  We need to flush layout to initiate any such loads so that
  // if mReady is currently resolved we replace it with a new pending Promise.
  // (That replacement will happen under this flush call.)
  if (!ReadyPromiseIsPending() && mDocument) {
    mDocument->FlushPendingNotifications(FlushType::Layout);
  }

  if (!mReady) {
    nsCOMPtr<nsIGlobalObject> global = GetParentObject();
    mReady = Promise::Create(global, aRv);
    if (!mReady) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    if (mResolveLazilyCreatedReadyPromise) {
      mReady->MaybeResolve(this);
      mResolveLazilyCreatedReadyPromise = false;
    }
  }

  return mReady;
}

FontFaceSetLoadStatus FontFaceSet::Status() {
  FlushUserFontSet();
  return mStatus;
}

#ifdef DEBUG
bool FontFaceSet::HasRuleFontFace(FontFace* aFontFace) {
  for (size_t i = 0; i < mRuleFaces.Length(); i++) {
    if (mRuleFaces[i].mFontFace == aFontFace) {
      return true;
    }
  }
  return false;
}
#endif

static bool IsPdfJs(nsIPrincipal* aPrincipal) {
  if (!aPrincipal) {
    return false;
  }
  nsCOMPtr<nsIURI> uri;
  aPrincipal->GetURI(getter_AddRefs(uri));
  return uri && uri->GetSpecOrDefault().EqualsLiteral(
                    "resource://pdf.js/web/viewer.html");
}

void FontFaceSet::Add(FontFace& aFontFace, ErrorResult& aRv) {
  FlushUserFontSet();

  if (aFontFace.IsInFontFaceSet(this)) {
    return;
  }

  if (aFontFace.HasRule()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_MODIFICATION_ERR);
    return;
  }

  aFontFace.AddFontFaceSet(this);

#ifdef DEBUG
  for (const FontFaceRecord& rec : mNonRuleFaces) {
    MOZ_ASSERT(rec.mFontFace != &aFontFace,
               "FontFace should not occur in mNonRuleFaces twice");
  }
#endif

  FontFaceRecord* rec = mNonRuleFaces.AppendElement();
  rec->mFontFace = &aFontFace;
  rec->mOrigin = Nothing();
  rec->mLoadEventShouldFire =
      aFontFace.Status() == FontFaceLoadStatus::Unloaded ||
      aFontFace.Status() == FontFaceLoadStatus::Loading;

  mNonRuleFacesDirty = true;
  MarkUserFontSetDirty();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingStarted();
  RefPtr<dom::Document> clonedDoc = mDocument->GetLatestStaticClone();
  if (clonedDoc) {
    // The document is printing, copy the font to the static clone as well.
    nsCOMPtr<nsIPrincipal> principal = mDocument->GetPrincipal();
    if (principal->IsSystemPrincipal() || IsPdfJs(principal)) {
      ErrorResult rv;
      clonedDoc->Fonts()->Add(aFontFace, rv);
      MOZ_ASSERT(!rv.Failed());
    }
  }
}

void FontFaceSet::Clear() {
  FlushUserFontSet();

  if (mNonRuleFaces.IsEmpty()) {
    return;
  }

  for (size_t i = 0; i < mNonRuleFaces.Length(); i++) {
    FontFace* f = mNonRuleFaces[i].mFontFace;
    f->RemoveFontFaceSet(this);
  }

  mNonRuleFaces.Clear();
  mNonRuleFacesDirty = true;
  MarkUserFontSetDirty();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingFinished();
}

bool FontFaceSet::Delete(FontFace& aFontFace) {
  FlushUserFontSet();

  if (aFontFace.HasRule()) {
    return false;
  }

  bool removed = false;
  for (size_t i = 0; i < mNonRuleFaces.Length(); i++) {
    if (mNonRuleFaces[i].mFontFace == &aFontFace) {
      mNonRuleFaces.RemoveElementAt(i);
      removed = true;
      break;
    }
  }
  if (!removed) {
    return false;
  }

  aFontFace.RemoveFontFaceSet(this);

  mNonRuleFacesDirty = true;
  MarkUserFontSetDirty();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingFinished();
  return true;
}

bool FontFaceSet::HasAvailableFontFace(FontFace* aFontFace) {
  return aFontFace->IsInFontFaceSet(this);
}

bool FontFaceSet::Has(FontFace& aFontFace) {
  FlushUserFontSet();

  return HasAvailableFontFace(&aFontFace);
}

FontFace* FontFaceSet::GetFontFaceAt(uint32_t aIndex) {
  FlushUserFontSet();

  if (aIndex < mRuleFaces.Length()) {
    return mRuleFaces[aIndex].mFontFace;
  }

  aIndex -= mRuleFaces.Length();
  if (aIndex < mNonRuleFaces.Length()) {
    return mNonRuleFaces[aIndex].mFontFace;
  }

  return nullptr;
}

uint32_t FontFaceSet::Size() {
  FlushUserFontSet();

  // Web IDL objects can only expose array index properties up to INT32_MAX.

  size_t total = mRuleFaces.Length() + mNonRuleFaces.Length();
  return std::min<size_t>(total, INT32_MAX);
}

already_AddRefed<FontFaceSetIterator> FontFaceSet::Entries() {
  RefPtr<FontFaceSetIterator> it = new FontFaceSetIterator(this, true);
  return it.forget();
}

already_AddRefed<FontFaceSetIterator> FontFaceSet::Values() {
  RefPtr<FontFaceSetIterator> it = new FontFaceSetIterator(this, false);
  return it.forget();
}

void FontFaceSet::ForEach(JSContext* aCx, FontFaceSetForEachCallback& aCallback,
                          JS::Handle<JS::Value> aThisArg, ErrorResult& aRv) {
  JS::Rooted<JS::Value> thisArg(aCx, aThisArg);
  for (size_t i = 0; i < Size(); i++) {
    RefPtr<FontFace> face = GetFontFaceAt(i);
    aCallback.Call(thisArg, *face, *face, *this, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

void FontFaceSet::RemoveLoader(nsFontFaceLoader* aLoader) {
  mLoaders.RemoveEntry(aLoader);
}

nsresult FontFaceSet::StartLoad(gfxUserFontEntry* aUserFontEntry,
                                const gfxFontFaceSrc* aFontFaceSrc) {
  nsresult rv;

  nsCOMPtr<nsIStreamLoader> streamLoader;
  nsCOMPtr<nsILoadGroup> loadGroup(mDocument->GetDocumentLoadGroup());
  gfxFontSrcPrincipal* principal = aUserFontEntry->GetPrincipal();

  nsCOMPtr<nsIChannel> channel;
  // Note we are calling NS_NewChannelWithTriggeringPrincipal() with both a
  // node and a principal.  This is because the document where the font is
  // being loaded might have a different origin from the principal of the
  // stylesheet that initiated the font load.
  rv = NS_NewChannelWithTriggeringPrincipal(
      getter_AddRefs(channel), aFontFaceSrc->mURI->get(), mDocument,
      principal ? principal->get() : nullptr,
      nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS, nsIContentPolicy::TYPE_FONT,
      nullptr,  // PerformanceStorage
      loadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsFontFaceLoader> fontLoader = new nsFontFaceLoader(
      aUserFontEntry, aFontFaceSrc->mURI->get(), this, channel);
  mLoaders.PutEntry(fontLoader);

  if (LOG_ENABLED()) {
    LOG(
        ("userfonts (%p) download start - font uri: (%s) "
         "referrer uri: (%s)\n",
         fontLoader.get(), aFontFaceSrc->mURI->GetSpecOrDefault().get(),
         aFontFaceSrc->mReferrer
             ? aFontFaceSrc->mReferrer->GetSpecOrDefault().get()
             : ""));
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    nsCOMPtr<nsIReferrerInfo> referrerInfo = new mozilla::dom::ReferrerInfo(
        aFontFaceSrc->mReferrer, aFontFaceSrc->mReferrerPolicy);
    rv = httpChannel->SetReferrerInfoWithoutClone(referrerInfo);
    Unused << NS_WARN_IF(NS_FAILED(rv));

    nsAutoCString accept("application/font-woff;q=0.9,*/*;q=0.8");
    if (Preferences::GetBool(GFX_PREF_WOFF2_ENABLED)) {
      accept.InsertLiteral("application/font-woff2;q=1.0,", 0);
    }
    rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"), accept,
                                       false);
    NS_ENSURE_SUCCESS(rv, rv);

    // For WOFF and WOFF2, we should tell servers/proxies/etc NOT to try
    // and apply additional compression at the content-encoding layer
    if (aFontFaceSrc->mFormatFlags & (gfxUserFontSet::FLAG_FORMAT_WOFF |
                                      gfxUserFontSet::FLAG_FORMAT_WOFF2)) {
      rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept-Encoding"),
                                         NS_LITERAL_CSTRING("identity"), false);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  nsCOMPtr<nsISupportsPriority> priorityChannel(do_QueryInterface(channel));
  if (priorityChannel) {
    priorityChannel->AdjustPriority(nsISupportsPriority::PRIORITY_HIGH);
  }

  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(channel));
  if (cos) {
    cos->AddClassFlags(nsIClassOfService::TailForbidden);
  }

  rv = NS_NewStreamLoader(getter_AddRefs(streamLoader), fontLoader, fontLoader);
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::net::PredictorLearn(
      aFontFaceSrc->mURI->get(), mDocument->GetDocumentURI(),
      nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE, loadGroup);

  rv = channel->AsyncOpen(streamLoader);
  if (NS_FAILED(rv)) {
    fontLoader->DropChannel();  // explicitly need to break ref cycle
  }

  if (NS_SUCCEEDED(rv)) {
    fontLoader->StartedLoading(streamLoader);
    // let the font entry remember the loader, in case we need to cancel it
    aUserFontEntry->SetLoader(fontLoader);
  }

  return rv;
}

bool FontFaceSet::UpdateRules(const nsTArray<nsFontFaceRuleContainer>& aRules) {
  MOZ_ASSERT(mUserFontSet);

  // If there was a change to the mNonRuleFaces array, then there could
  // have been a modification to the user font set.
  bool modified = mNonRuleFacesDirty;
  mNonRuleFacesDirty = false;

  // reuse existing FontFace objects mapped to rules already
  nsDataHashtable<nsPtrHashKey<RawServoFontFaceRule>, FontFace*> ruleFaceMap;
  for (size_t i = 0, i_end = mRuleFaces.Length(); i < i_end; ++i) {
    FontFace* f = mRuleFaces[i].mFontFace;
    if (!f) {
      continue;
    }
    ruleFaceMap.Put(f->GetRule(), f);
  }

  // The @font-face rules that make up the user font set have changed,
  // so we need to update the set. However, we want to preserve existing
  // font entries wherever possible, so that we don't discard and then
  // re-download resources in the (common) case where at least some of the
  // same rules are still present.

  nsTArray<FontFaceRecord> oldRecords;
  mRuleFaces.SwapElements(oldRecords);

  // Remove faces from the font family records; we need to re-insert them
  // because we might end up with faces in a different order even if they're
  // the same font entries as before. (The order can affect font selection
  // where multiple faces match the requested style, perhaps with overlapping
  // unicode-range coverage.)
  for (auto it = mUserFontSet->mFontFamilies.Iter(); !it.Done(); it.Next()) {
    it.Data()->DetachFontEntries();
  }

  // Sometimes aRules has duplicate @font-face rules in it; we should make
  // that not happen, but in the meantime, don't try to insert the same
  // FontFace object more than once into mRuleFaces.  We track which
  // ones we've handled in this table.
  nsTHashtable<nsPtrHashKey<RawServoFontFaceRule>> handledRules;

  for (size_t i = 0, i_end = aRules.Length(); i < i_end; ++i) {
    // Insert each FontFace objects for each rule into our list, migrating old
    // font entries if possible rather than creating new ones; set  modified  to
    // true if we detect that rule ordering has changed, or if a new entry is
    // created.
    RawServoFontFaceRule* rule = aRules[i].mRule;
    if (!handledRules.EnsureInserted(rule)) {
      // rule was already present in the hashtable
      continue;
    }
    RefPtr<FontFace> f = ruleFaceMap.Get(rule);
    if (!f.get()) {
      f = FontFace::CreateForRule(GetParentObject(), this, rule);
    }
    InsertRuleFontFace(f, aRules[i].mOrigin, oldRecords, modified);
  }

  for (size_t i = 0, i_end = mNonRuleFaces.Length(); i < i_end; ++i) {
    // Do the same for the non rule backed FontFace objects.
    InsertNonRuleFontFace(mNonRuleFaces[i].mFontFace, modified);
  }

  // Remove any residual families that have no font entries (i.e., they were
  // not defined at all by the updated set of @font-face rules).
  for (auto it = mUserFontSet->mFontFamilies.Iter(); !it.Done(); it.Next()) {
    if (it.Data()->GetFontList().IsEmpty()) {
      it.Remove();
    }
  }

  // If any FontFace objects for rules are left in the old list, note that the
  // set has changed (even if the new set was built entirely by migrating old
  // font entries).
  if (oldRecords.Length() > 0) {
    modified = true;
    // Any in-progress loaders for obsolete rules should be cancelled,
    // as the resource being downloaded will no longer be required.
    // We need to explicitly remove any loaders here, otherwise the loaders
    // will keep their "orphaned" font entries alive until they complete,
    // even after the oldRules array is deleted.
    //
    // XXX Now that it is possible for the author to hold on to a rule backed
    // FontFace object, we shouldn't cancel loading here; instead we should do
    // it when the FontFace is GCed, if we can detect that.
    size_t count = oldRecords.Length();
    for (size_t i = 0; i < count; ++i) {
      RefPtr<FontFace> f = oldRecords[i].mFontFace;
      gfxUserFontEntry* userFontEntry = f->GetUserFontEntry();
      if (userFontEntry) {
        nsFontFaceLoader* loader = userFontEntry->GetLoader();
        if (loader) {
          loader->Cancel();
          RemoveLoader(loader);
        }
      }

      // Any left over FontFace objects should also cease being rule backed.
      f->DisconnectFromRule();
    }
  }

  if (modified) {
    IncrementGeneration(true);
    mHasLoadingFontFacesIsDirty = true;
    CheckLoadingStarted();
    CheckLoadingFinished();
  }

  // if local rules needed to be rebuilt, they have been rebuilt at this point
  if (mUserFontSet->mRebuildLocalRules) {
    mUserFontSet->mLocalRulesUsed = false;
    mUserFontSet->mRebuildLocalRules = false;
  }

  if (LOG_ENABLED() && !mRuleFaces.IsEmpty()) {
    LOG(("userfonts (%p) userfont rules update (%s) rule count: %d",
         mUserFontSet.get(), (modified ? "modified" : "not modified"),
         (int)(mRuleFaces.Length())));
  }

  return modified;
}

void FontFaceSet::IncrementGeneration(bool aIsRebuild) {
  MOZ_ASSERT(mUserFontSet);
  mUserFontSet->IncrementGeneration(aIsRebuild);
}

void FontFaceSet::InsertNonRuleFontFace(FontFace* aFontFace,
                                        bool& aFontSetModified) {
  nsAtom* fontFamily = aFontFace->GetFamilyName();
  if (!fontFamily) {
    // If there is no family name, this rule cannot contribute a
    // usable font, so there is no point in processing it further.
    return;
  }

  nsAtomCString family(fontFamily);

  // Just create a new font entry if we haven't got one already.
  if (!aFontFace->GetUserFontEntry()) {
    // XXX Should we be checking mUserFontSet->mLocalRulesUsed like
    // InsertRuleFontFace does?
    RefPtr<gfxUserFontEntry> entry = FindOrCreateUserFontEntryFromFontFace(
        family, aFontFace, StyleOrigin::Author);
    if (!entry) {
      return;
    }
    aFontFace->SetUserFontEntry(entry);
  }

  aFontSetModified = true;
  mUserFontSet->AddUserFontEntry(family, aFontFace->GetUserFontEntry());
}

void FontFaceSet::InsertRuleFontFace(FontFace* aFontFace,
                                     StyleOrigin aSheetType,
                                     nsTArray<FontFaceRecord>& aOldRecords,
                                     bool& aFontSetModified) {
  nsAtom* fontFamily = aFontFace->GetFamilyName();
  if (!fontFamily) {
    // If there is no family name, this rule cannot contribute a
    // usable font, so there is no point in processing it further.
    return;
  }

  bool remove = false;
  size_t removeIndex;

  nsAtomCString family(fontFamily);

  // This is a rule backed FontFace.  First, we check in aOldRecords; if
  // the FontFace for the rule exists there, just move it to the new record
  // list, and put the entry into the appropriate family.
  for (size_t i = 0; i < aOldRecords.Length(); ++i) {
    FontFaceRecord& rec = aOldRecords[i];

    if (rec.mFontFace == aFontFace && rec.mOrigin == Some(aSheetType)) {
      // if local rules were used, don't use the old font entry
      // for rules containing src local usage
      if (mUserFontSet->mLocalRulesUsed && mUserFontSet->mRebuildLocalRules) {
        if (aFontFace->HasLocalSrc()) {
          // Remove the old record, but wait to see if we successfully create a
          // new user font entry below.
          remove = true;
          removeIndex = i;
          break;
        }
      }

      gfxUserFontEntry* entry = rec.mFontFace->GetUserFontEntry();
      MOZ_ASSERT(entry, "FontFace should have a gfxUserFontEntry by now");

      mUserFontSet->AddUserFontEntry(family, entry);

      MOZ_ASSERT(!HasRuleFontFace(rec.mFontFace),
                 "FontFace should not occur in mRuleFaces twice");

      mRuleFaces.AppendElement(rec);
      aOldRecords.RemoveElementAt(i);
      // note the set has been modified if an old rule was skipped to find
      // this one - something has been dropped, or ordering changed
      if (i > 0) {
        aFontSetModified = true;
      }
      return;
    }
  }

  // this is a new rule:
  RefPtr<gfxUserFontEntry> entry =
      FindOrCreateUserFontEntryFromFontFace(family, aFontFace, aSheetType);

  if (!entry) {
    return;
  }

  if (remove) {
    // Although we broke out of the aOldRecords loop above, since we found
    // src local usage, and we're not using the old user font entry, we still
    // are adding a record to mRuleFaces with the same FontFace object.
    // Remove the old record so that we don't have the same FontFace listed
    // in both mRuleFaces and oldRecords, which would cause us to call
    // DisconnectFromRule on a FontFace that should still be rule backed.
    aOldRecords.RemoveElementAt(removeIndex);
  }

  FontFaceRecord rec;
  rec.mFontFace = aFontFace;
  rec.mOrigin = Some(aSheetType);
  rec.mLoadEventShouldFire =
      aFontFace->Status() == FontFaceLoadStatus::Unloaded ||
      aFontFace->Status() == FontFaceLoadStatus::Loading;

  aFontFace->SetUserFontEntry(entry);

  MOZ_ASSERT(!HasRuleFontFace(aFontFace),
             "FontFace should not occur in mRuleFaces twice");

  mRuleFaces.AppendElement(rec);

  // this was a new rule and font entry, so note that the set was modified
  aFontSetModified = true;

  // Add the entry to the end of the list.  If an existing userfont entry was
  // returned by FindOrCreateUserFontEntryFromFontFace that was already stored
  // on the family, gfxUserFontFamily::AddFontEntry(), which AddUserFontEntry
  // calls, will automatically remove the earlier occurrence of the same
  // userfont entry.
  mUserFontSet->AddUserFontEntry(family, entry);
}

/* static */
already_AddRefed<gfxUserFontEntry>
FontFaceSet::FindOrCreateUserFontEntryFromFontFace(FontFace* aFontFace) {
  nsAtom* fontFamily = aFontFace->GetFamilyName();
  if (!fontFamily) {
    // If there is no family name, this rule cannot contribute a
    // usable font, so there is no point in processing it further.
    return nullptr;
  }

  return FindOrCreateUserFontEntryFromFontFace(nsAtomCString(fontFamily),
                                               aFontFace, StyleOrigin::Author);
}

static WeightRange GetWeightRangeForDescriptor(
    const Maybe<StyleComputedFontWeightRange>& aVal,
    gfxFontEntry::RangeFlags& aRangeFlags) {
  if (!aVal) {
    aRangeFlags |= gfxFontEntry::RangeFlags::eAutoWeight;
    return WeightRange(FontWeight::Normal());
  }
  return WeightRange(FontWeight(aVal->_0), FontWeight(aVal->_1));
}

static SlantStyleRange GetStyleRangeForDescriptor(
    const Maybe<StyleComputedFontStyleDescriptor>& aVal,
    gfxFontEntry::RangeFlags& aRangeFlags) {
  if (!aVal) {
    aRangeFlags |= gfxFontEntry::RangeFlags::eAutoSlantStyle;
    return SlantStyleRange(FontSlantStyle::Normal());
  }
  auto& val = *aVal;
  switch (val.tag) {
    case StyleComputedFontStyleDescriptor::Tag::Normal:
      return SlantStyleRange(FontSlantStyle::Normal());
    case StyleComputedFontStyleDescriptor::Tag::Italic:
      return SlantStyleRange(FontSlantStyle::Italic());
    case StyleComputedFontStyleDescriptor::Tag::Oblique:
      return SlantStyleRange(FontSlantStyle::Oblique(val.AsOblique()._0),
                             FontSlantStyle::Oblique(val.AsOblique()._1));
  }
  MOZ_ASSERT_UNREACHABLE("How?");
  return SlantStyleRange(FontSlantStyle::Normal());
}

static StretchRange GetStretchRangeForDescriptor(
    const Maybe<StyleComputedFontStretchRange>& aVal,
    gfxFontEntry::RangeFlags& aRangeFlags) {
  if (!aVal) {
    aRangeFlags |= gfxFontEntry::RangeFlags::eAutoStretch;
    return StretchRange(FontStretch::Normal());
  }
  return StretchRange(FontStretch::FromStyle(aVal->_0),
                      FontStretch::FromStyle(aVal->_1));
}

// TODO(emilio): Should this take an nsAtom* aFamilyName instead?
//
// All callers have one handy.
/* static */
already_AddRefed<gfxUserFontEntry>
FontFaceSet::FindOrCreateUserFontEntryFromFontFace(
    const nsACString& aFamilyName, FontFace* aFontFace, StyleOrigin aOrigin) {
  FontFaceSet* set = aFontFace->GetPrimaryFontFaceSet();

  uint32_t languageOverride = NO_FONT_LANGUAGE_OVERRIDE;
  StyleFontDisplay fontDisplay = StyleFontDisplay::Auto;

  gfxFontEntry::RangeFlags rangeFlags = gfxFontEntry::RangeFlags::eNoFlags;

  // set up weight
  WeightRange weight =
      GetWeightRangeForDescriptor(aFontFace->GetFontWeight(), rangeFlags);

  // set up stretch
  StretchRange stretch =
      GetStretchRangeForDescriptor(aFontFace->GetFontStretch(), rangeFlags);

  // set up font style
  SlantStyleRange italicStyle =
      GetStyleRangeForDescriptor(aFontFace->GetFontStyle(), rangeFlags);

  // set up font display
  if (Maybe<StyleFontDisplay> display = aFontFace->GetFontDisplay()) {
    fontDisplay = *display;
  }

  // set up font features
  nsTArray<gfxFontFeature> featureSettings;
  aFontFace->GetFontFeatureSettings(featureSettings);

  // set up font variations
  nsTArray<gfxFontVariation> variationSettings;
  aFontFace->GetFontVariationSettings(variationSettings);

  // set up font language override
  if (Maybe<StyleFontLanguageOverride> descriptor =
          aFontFace->GetFontLanguageOverride()) {
    languageOverride = descriptor->_0;
  }

  // set up unicode-range
  gfxCharacterMap* unicodeRanges = aFontFace->GetUnicodeRangeAsCharacterMap();

  RefPtr<gfxUserFontEntry> existingEntry = aFontFace->GetUserFontEntry();
  if (existingEntry) {
    // aFontFace already has a user font entry, so we update its attributes
    // rather than creating a new one.
    existingEntry->UpdateAttributes(weight, stretch, italicStyle,
                                    featureSettings, variationSettings,
                                    languageOverride, unicodeRanges,
                                    fontDisplay, rangeFlags);
    // If the family name has changed, remove the entry from its current family
    // and clear the mFamilyName field so it can be reset when added to a new
    // family.
    if (!existingEntry->mFamilyName.IsEmpty() &&
        existingEntry->mFamilyName != aFamilyName) {
      gfxUserFontFamily* family =
        set->GetUserFontSet()->LookupFamily(existingEntry->mFamilyName);
      if (family) {
        family->RemoveFontEntry(existingEntry);
      }
      existingEntry->mFamilyName.Truncate(0);
    }
    return existingEntry.forget();
  }

  // set up src array
  nsTArray<gfxFontFaceSrc> srcArray;

  if (aFontFace->HasFontData()) {
    gfxFontFaceSrc* face = srcArray.AppendElement();
    if (!face) {
      return nullptr;
    }

    face->mSourceType = gfxFontFaceSrc::eSourceType_Buffer;
    face->mBuffer = aFontFace->CreateBufferSource();
    face->mReferrerPolicy = mozilla::net::RP_Unset;
  } else {
    AutoTArray<StyleFontFaceSourceListComponent, 8> sourceListComponents;
    aFontFace->GetSources(sourceListComponents);
    size_t len = sourceListComponents.Length();
    for (size_t i = 0; i < len; ++i) {
      gfxFontFaceSrc* face = srcArray.AppendElement();
      const auto& component = sourceListComponents[i];
      switch (component.tag) {
        case StyleFontFaceSourceListComponent::Tag::Local: {
          nsAtom* atom = component.AsLocal();
          face->mLocalName.Append(nsAtomCString(atom));
          face->mSourceType = gfxFontFaceSrc::eSourceType_Local;
          face->mURI = nullptr;
          face->mFormatFlags = 0;
          face->mReferrerPolicy = mozilla::net::RP_Unset;
          break;
        }
        case StyleFontFaceSourceListComponent::Tag::Url: {
          face->mSourceType = gfxFontFaceSrc::eSourceType_URL;
          const URLValue* url = component.AsUrl();
          nsIURI* uri = url->GetURI();
          face->mURI = uri ? new gfxFontSrcURI(uri) : nullptr;
          URLExtraData* extraData = url->ExtraData();
          face->mReferrer = extraData->GetReferrer();
          face->mReferrerPolicy = extraData->GetReferrerPolicy();
          face->mOriginPrincipal =
              new gfxFontSrcPrincipal(extraData->Principal());

          // agent and user stylesheets are treated slightly differently,
          // the same-site origin check and access control headers are
          // enforced against the sheet principal rather than the document
          // principal to allow user stylesheets to include @font-face rules
          face->mUseOriginPrincipal =
              aOrigin == StyleOrigin::User || aOrigin == StyleOrigin::UserAgent;

          face->mLocalName.Truncate();
          face->mFormatFlags = 0;

          while (i + 1 < len) {
            const auto& maybeFontFormat = sourceListComponents[i + 1];
            if (maybeFontFormat.tag !=
                StyleFontFaceSourceListComponent::Tag::FormatHint) {
              break;
            }

            nsDependentCSubstring valueString(
                reinterpret_cast<const char*>(
                    maybeFontFormat.format_hint.utf8_bytes),
                maybeFontFormat.format_hint.length);

            if (valueString.LowerCaseEqualsASCII("woff")) {
              face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_WOFF;
            } else if (Preferences::GetBool(GFX_PREF_WOFF2_ENABLED) &&
                       valueString.LowerCaseEqualsASCII("woff2")) {
              face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_WOFF2;
            } else if (valueString.LowerCaseEqualsASCII("opentype")) {
              face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_OPENTYPE;
            } else if (valueString.LowerCaseEqualsASCII("truetype")) {
              face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_TRUETYPE;
            } else if (valueString.LowerCaseEqualsASCII("truetype-aat")) {
              face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_TRUETYPE_AAT;
            } else if (valueString.LowerCaseEqualsASCII("embedded-opentype")) {
              face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_EOT;
            } else if (valueString.LowerCaseEqualsASCII("svg")) {
              face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_SVG;
            } else if (StaticPrefs::layout_css_font_variations_enabled() &&
                       valueString.LowerCaseEqualsASCII("woff-variations")) {
              face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_WOFF_VARIATIONS;
            } else if (StaticPrefs::layout_css_font_variations_enabled() &&
                       Preferences::GetBool(GFX_PREF_WOFF2_ENABLED) &&
                       valueString.LowerCaseEqualsASCII("woff2-variations")) {
              face->mFormatFlags |=
                  gfxUserFontSet::FLAG_FORMAT_WOFF2_VARIATIONS;
            } else if (StaticPrefs::layout_css_font_variations_enabled() &&
                       valueString.LowerCaseEqualsASCII(
                           "opentype-variations")) {
              face->mFormatFlags |=
                  gfxUserFontSet::FLAG_FORMAT_OPENTYPE_VARIATIONS;
            } else if (StaticPrefs::layout_css_font_variations_enabled() &&
                       valueString.LowerCaseEqualsASCII(
                           "truetype-variations")) {
              face->mFormatFlags |=
                  gfxUserFontSet::FLAG_FORMAT_TRUETYPE_VARIATIONS;
            } else {
              // unknown format specified, mark to distinguish from the
              // case where no format hints are specified
              face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_UNKNOWN;
            }
            i++;
          }
          if (!face->mURI) {
            // if URI not valid, omit from src array
            srcArray.RemoveLastElement();
            NS_WARNING("null url in @font-face rule");
            continue;
          }
          break;
        }
        case StyleFontFaceSourceListComponent::Tag::FormatHint:
          MOZ_ASSERT_UNREACHABLE(
              "Should always come after a URL source, and be consumed already");
          break;
      }
    }
  }

  if (srcArray.IsEmpty()) {
    return nullptr;
  }

  RefPtr<gfxUserFontEntry> entry = set->mUserFontSet->FindOrCreateUserFontEntry(
      aFamilyName, srcArray, weight, stretch, italicStyle, featureSettings,
      variationSettings, languageOverride, unicodeRanges, fontDisplay,
      rangeFlags);

  return entry.forget();
}

RawServoFontFaceRule* FontFaceSet::FindRuleForEntry(gfxFontEntry* aFontEntry) {
  NS_ASSERTION(!aFontEntry->mIsUserFontContainer, "only platform font entries");
  for (uint32_t i = 0; i < mRuleFaces.Length(); ++i) {
    FontFace* f = mRuleFaces[i].mFontFace;
    gfxUserFontEntry* entry = f->GetUserFontEntry();
    if (entry && entry->GetPlatformFontEntry() == aFontEntry) {
      return f->GetRule();
    }
  }
  return nullptr;
}

RawServoFontFaceRule* FontFaceSet::FindRuleForUserFontEntry(
    gfxUserFontEntry* aUserFontEntry) {
  for (uint32_t i = 0; i < mRuleFaces.Length(); ++i) {
    FontFace* f = mRuleFaces[i].mFontFace;
    if (f->GetUserFontEntry() == aUserFontEntry) {
      return f->GetRule();
    }
  }
  return nullptr;
}

nsresult FontFaceSet::LogMessage(gfxUserFontEntry* aUserFontEntry,
                                 const char* aMessage, uint32_t aFlags,
                                 nsresult aStatus) {
  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString familyName;
  nsAutoCString fontURI;
  aUserFontEntry->GetFamilyNameAndURIForLogging(familyName, fontURI);

  nsAutoCString weightString;
  aUserFontEntry->Weight().ToString(weightString);
  nsAutoCString stretchString;
  aUserFontEntry->Stretch().ToString(stretchString);
  nsPrintfCString message(
      "downloadable font: %s "
      "(font-family: \"%s\" style:%s weight:%s stretch:%s src index:%d)",
      aMessage, familyName.get(),
      aUserFontEntry->IsItalic() ? "italic" : "normal",  // XXX todo: oblique?
      weightString.get(), stretchString.get(), aUserFontEntry->GetSrcIndex());

  if (NS_FAILED(aStatus)) {
    message.AppendLiteral(": ");
    switch (aStatus) {
      case NS_ERROR_DOM_BAD_URI:
        message.AppendLiteral("bad URI or cross-site access not allowed");
        break;
      case NS_ERROR_CONTENT_BLOCKED:
        message.AppendLiteral("content blocked");
        break;
      default:
        message.AppendLiteral("status=");
        message.AppendInt(static_cast<uint32_t>(aStatus));
        break;
    }
  }
  message.AppendLiteral(" source: ");
  message.Append(fontURI);

  if (LOG_ENABLED()) {
    LOG(("userfonts (%p) %s", mUserFontSet.get(), message.get()));
  }

  // try to give the user an indication of where the rule came from
  RawServoFontFaceRule* rule = FindRuleForUserFontEntry(aUserFontEntry);
  nsString href;
  nsString text;
  uint32_t line = 0;
  uint32_t column = 0;
  if (rule) {
    Servo_FontFaceRule_GetCssText(rule, &text);
    Servo_FontFaceRule_GetSourceLocation(rule, &line, &column);
    // FIXME We need to figure out an approach to get the style sheet
    // of this raw rule. See bug 1450903.
#if 0
    StyleSheet* sheet = rule->GetStyleSheet();
    // if the style sheet is removed while the font is loading can be null
    if (sheet) {
      nsCString spec = sheet->GetSheetURI()->GetSpecOrDefault();
      CopyUTF8toUTF16(spec, href);
    } else {
      NS_WARNING("null parent stylesheet for @font-face rule");
      href.AssignLiteral("unknown");
    }
#endif
    href.AssignLiteral("unknown");
  }

  nsresult rv;
  nsCOMPtr<nsIScriptError> scriptError =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t innerWindowID = mDocument->InnerWindowID();
  rv = scriptError->InitWithWindowID(NS_ConvertUTF8toUTF16(message),
                                     href,  // file
                                     text,  // src line
                                     line, column,
                                     aFlags,        // flags
                                     "CSS Loader",  // category (make separate?)
                                     innerWindowID);
  if (NS_SUCCEEDED(rv)) {
    console->LogMessage(scriptError);
  }

  return NS_OK;
}

void FontFaceSet::CacheFontLoadability() {
  if (!mUserFontSet) {
    return;
  }

  // TODO(emilio): We could do it a bit more incrementally maybe?
  for (auto iter = mUserFontSet->mFontFamilies.Iter(); !iter.Done();
       iter.Next()) {
    for (const gfxFontEntry* entry : iter.Data()->GetFontList()) {
      if (!entry->mIsUserFontContainer) {
        continue;
      }

      const auto& sourceList =
          static_cast<const gfxUserFontEntry*>(entry)->SourceList();
      for (const gfxFontFaceSrc& src : sourceList) {
        if (src.mSourceType != gfxFontFaceSrc::eSourceType_URL) {
          continue;
        }
        mAllowedFontLoads.LookupForAdd(&src).OrInsert(
            [&] { return IsFontLoadAllowed(src); });
      }
    }
  }
}

bool FontFaceSet::IsFontLoadAllowed(const gfxFontFaceSrc& aSrc) {
  MOZ_ASSERT(aSrc.mSourceType == gfxFontFaceSrc::eSourceType_URL);

  if (ServoStyleSet::IsInServoTraversal()) {
    bool* entry = mAllowedFontLoads.GetValue(&aSrc);
    MOZ_DIAGNOSTIC_ASSERT(entry, "Missed an update?");
    return entry ? *entry : false;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!mUserFontSet) {
    return false;
  }

  gfxFontSrcPrincipal* gfxPrincipal = aSrc.mURI->InheritsSecurityContext()
                                          ? nullptr
                                          : aSrc.LoadPrincipal(*mUserFontSet);

  nsIPrincipal* principal = gfxPrincipal ? gfxPrincipal->get() : nullptr;

  nsCOMPtr<nsILoadInfo> secCheckLoadInfo = new net::LoadInfo(
      mDocument->NodePrincipal(),  // loading principal
      principal,                   // triggering principal
      mDocument, nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
      nsIContentPolicy::TYPE_FONT);

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(aSrc.mURI->get(), secCheckLoadInfo,
                                          EmptyCString(),  // mime type
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy());

  return NS_SUCCEEDED(rv) && NS_CP_ACCEPTED(shouldLoad);
}

void FontFaceSet::DispatchFontLoadViolations(
    nsTArray<nsCOMPtr<nsIRunnable>>& aViolations) {
  if (XRE_IsContentProcess()) {
    nsCOMPtr<nsIEventTarget> eventTarget =
        mDocument->EventTargetFor(TaskCategory::Other);
    for (nsIRunnable* runnable : aViolations) {
      eventTarget->Dispatch(do_AddRef(runnable), NS_DISPATCH_NORMAL);
    }
  } else {
    for (nsIRunnable* runnable : aViolations) {
      NS_DispatchToMainThread(do_AddRef(runnable));
    }
  }
}

nsresult FontFaceSet::SyncLoadFontData(gfxUserFontEntry* aFontToLoad,
                                       const gfxFontFaceSrc* aFontFaceSrc,
                                       uint8_t*& aBuffer,
                                       uint32_t& aBufferLength) {
  nsresult rv;

  gfxFontSrcPrincipal* principal = aFontToLoad->GetPrincipal();

  nsCOMPtr<nsIChannel> channel;
  // Note we are calling NS_NewChannelWithTriggeringPrincipal() with both a
  // node and a principal.  This is because the document where the font is
  // being loaded might have a different origin from the principal of the
  // stylesheet that initiated the font load.
  // Further, we only get here for data: loads, so it doesn't really matter
  // whether we use SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS or not, to be more
  // restrictive we use SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS.
  rv = NS_NewChannelWithTriggeringPrincipal(
      getter_AddRefs(channel), aFontFaceSrc->mURI->get(), mDocument,
      principal ? principal->get() : nullptr,
      nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS,
      nsIContentPolicy::TYPE_FONT);

  NS_ENSURE_SUCCESS(rv, rv);

  // blocking stream is OK for data URIs
  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t bufferLength64;
  rv = stream->Available(&bufferLength64);
  NS_ENSURE_SUCCESS(rv, rv);
  if (bufferLength64 == 0) {
    return NS_ERROR_FAILURE;
  }
  if (bufferLength64 > UINT32_MAX) {
    return NS_ERROR_FILE_TOO_BIG;
  }
  aBufferLength = static_cast<uint32_t>(bufferLength64);

  // read all the decoded data
  aBuffer = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * aBufferLength));
  if (!aBuffer) {
    aBufferLength = 0;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t numRead, totalRead = 0;
  while (NS_SUCCEEDED(
             rv = stream->Read(reinterpret_cast<char*>(aBuffer + totalRead),
                               aBufferLength - totalRead, &numRead)) &&
         numRead != 0) {
    totalRead += numRead;
    if (totalRead > aBufferLength) {
      rv = NS_ERROR_FAILURE;
      break;
    }
  }

  // make sure there's a mime type
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString mimeType;
    rv = channel->GetContentType(mimeType);
    aBufferLength = totalRead;
  }

  if (NS_FAILED(rv)) {
    free(aBuffer);
    aBuffer = nullptr;
    aBufferLength = 0;
    return rv;
  }

  return NS_OK;
}

void FontFaceSet::OnFontFaceStatusChanged(FontFace* aFontFace) {
  AssertIsMainThreadOrServoFontMetricsLocked();

  MOZ_ASSERT(HasAvailableFontFace(aFontFace));

  mHasLoadingFontFacesIsDirty = true;

  if (aFontFace->Status() == FontFaceLoadStatus::Loading) {
    CheckLoadingStarted();
  } else {
    MOZ_ASSERT(aFontFace->Status() == FontFaceLoadStatus::Loaded ||
               aFontFace->Status() == FontFaceLoadStatus::Error);
    // When a font finishes downloading, nsPresContext::UserFontSetUpdated
    // will be called immediately afterwards to request a reflow of the
    // relevant elements in the document.  We want to wait until the reflow
    // request has been done before the FontFaceSet is marked as Loaded so
    // that we don't briefly set the FontFaceSet to Loaded and then Loading
    // again once the reflow is pending.  So we go around the event loop
    // and call CheckLoadingFinished() after the reflow has been queued.
    if (!mDelayedLoadCheck) {
      mDelayedLoadCheck = true;
      DispatchCheckLoadingFinishedAfterDelay();
    }
  }
}

void FontFaceSet::DispatchCheckLoadingFinishedAfterDelay() {
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (ServoStyleSet* set = ServoStyleSet::Current()) {
    // See comments in Gecko_GetFontMetrics.
    //
    // We can't just dispatch the runnable below if we're not on the main
    // thread, since it needs to take a strong reference to the FontFaceSet,
    // and being a DOM object, FontFaceSet doesn't support thread-safe
    // refcounting.
    set->AppendTask(
        PostTraversalTask::DispatchFontFaceSetCheckLoadingFinishedAfterDelay(
            this));
    return;
  }

  nsCOMPtr<nsIRunnable> checkTask =
      NewRunnableMethod("dom::FontFaceSet::CheckLoadingFinishedAfterDelay",
                        this, &FontFaceSet::CheckLoadingFinishedAfterDelay);
  mDocument->Dispatch(TaskCategory::Other, checkTask.forget());
}

void FontFaceSet::DidRefresh() { CheckLoadingFinished(); }

void FontFaceSet::CheckLoadingFinishedAfterDelay() {
  mDelayedLoadCheck = false;
  CheckLoadingFinished();
}

void FontFaceSet::CheckLoadingStarted() {
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (!HasLoadingFontFaces()) {
    return;
  }

  if (mStatus == FontFaceSetLoadStatus::Loading) {
    // We have already dispatched a loading event and replaced mReady
    // with a fresh, unresolved promise.
    return;
  }

  mStatus = FontFaceSetLoadStatus::Loading;
  DispatchLoadingEventAndReplaceReadyPromise();
}

void FontFaceSet::DispatchLoadingEventAndReplaceReadyPromise() {
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (ServoStyleSet* set = ServoStyleSet::Current()) {
    // See comments in Gecko_GetFontMetrics.
    //
    // We can't just dispatch the runnable below if we're not on the main
    // thread, since it needs to take a strong reference to the FontFaceSet,
    // and being a DOM object, FontFaceSet doesn't support thread-safe
    // refcounting.  (Also, the Promise object creation must be done on
    // the main thread.)
    set->AppendTask(
        PostTraversalTask::DispatchLoadingEventAndReplaceReadyPromise(this));
    return;
  }

  (new AsyncEventDispatcher(this, NS_LITERAL_STRING("loading"), CanBubble::eNo))
      ->PostDOMEvent();

  if (PrefEnabled()) {
    if (mReady && mReady->State() != Promise::PromiseState::Pending) {
      if (GetParentObject()) {
        ErrorResult rv;
        mReady = Promise::Create(GetParentObject(), rv);
      }
    }

    // We may previously have been in a state where all fonts had finished
    // loading and we'd set mResolveLazilyCreatedReadyPromise to make sure that
    // if we lazily create mReady for a consumer that we resolve it before
    // returning it.  We're now loading fonts, so we need to clear that flag.
    mResolveLazilyCreatedReadyPromise = false;
  }
}

void FontFaceSet::UpdateHasLoadingFontFaces() {
  mHasLoadingFontFacesIsDirty = false;
  mHasLoadingFontFaces = false;
  for (size_t i = 0; i < mRuleFaces.Length(); i++) {
    FontFace* f = mRuleFaces[i].mFontFace;
    if (f->Status() == FontFaceLoadStatus::Loading) {
      mHasLoadingFontFaces = true;
      return;
    }
  }
  for (size_t i = 0; i < mNonRuleFaces.Length(); i++) {
    if (mNonRuleFaces[i].mFontFace->Status() == FontFaceLoadStatus::Loading) {
      mHasLoadingFontFaces = true;
      return;
    }
  }
}

bool FontFaceSet::HasLoadingFontFaces() {
  if (mHasLoadingFontFacesIsDirty) {
    UpdateHasLoadingFontFaces();
  }
  return mHasLoadingFontFaces;
}

bool FontFaceSet::MightHavePendingFontLoads() {
  // Check for FontFace objects in the FontFaceSet that are still loading.
  if (HasLoadingFontFaces()) {
    return true;
  }

  // Check for pending restyles or reflows, as they might cause fonts to
  // load as new styles apply and text runs are rebuilt.
  nsPresContext* presContext = GetPresContext();
  if (presContext && presContext->HasPendingRestyleOrReflow()) {
    return true;
  }

  if (mDocument) {
    // We defer resolving mReady until the document as fully loaded.
    if (!mDocument->DidFireDOMContentLoaded()) {
      return true;
    }

    // And we also wait for any CSS style sheets to finish loading, as their
    // styles might cause new fonts to load.
    if (mDocument->CSSLoader()->HasPendingLoads()) {
      return true;
    }
  }

  return false;
}

void FontFaceSet::CheckLoadingFinished() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mDelayedLoadCheck) {
    // Wait until the runnable posted in OnFontFaceStatusChanged calls us.
    return;
  }

  if (!ReadyPromiseIsPending()) {
    // We've already resolved mReady (or set the flag to do that lazily) and
    // dispatched the loadingdone/loadingerror events.
    return;
  }

  if (MightHavePendingFontLoads()) {
    // We're not finished loading yet.
    return;
  }

  mStatus = FontFaceSetLoadStatus::Loaded;
  if (mReady) {
    mReady->MaybeResolve(this);
  } else {
    mResolveLazilyCreatedReadyPromise = true;
  }

  // Now dispatch the loadingdone/loadingerror events.
  nsTArray<OwningNonNull<FontFace>> loaded;
  nsTArray<OwningNonNull<FontFace>> failed;

  for (size_t i = 0; i < mRuleFaces.Length(); i++) {
    if (!mRuleFaces[i].mLoadEventShouldFire) {
      continue;
    }
    FontFace* f = mRuleFaces[i].mFontFace;
    if (f->Status() == FontFaceLoadStatus::Loaded) {
      loaded.AppendElement(*f);
      mRuleFaces[i].mLoadEventShouldFire = false;
    } else if (f->Status() == FontFaceLoadStatus::Error) {
      failed.AppendElement(*f);
      mRuleFaces[i].mLoadEventShouldFire = false;
    }
  }

  for (size_t i = 0; i < mNonRuleFaces.Length(); i++) {
    if (!mNonRuleFaces[i].mLoadEventShouldFire) {
      continue;
    }
    FontFace* f = mNonRuleFaces[i].mFontFace;
    if (f->Status() == FontFaceLoadStatus::Loaded) {
      loaded.AppendElement(*f);
      mNonRuleFaces[i].mLoadEventShouldFire = false;
    } else if (f->Status() == FontFaceLoadStatus::Error) {
      failed.AppendElement(*f);
      mNonRuleFaces[i].mLoadEventShouldFire = false;
    }
  }

  DispatchLoadingFinishedEvent(NS_LITERAL_STRING("loadingdone"),
                               std::move(loaded));

  if (!failed.IsEmpty()) {
    DispatchLoadingFinishedEvent(NS_LITERAL_STRING("loadingerror"),
                                 std::move(failed));
  }
}

void FontFaceSet::DispatchLoadingFinishedEvent(
    const nsAString& aType, nsTArray<OwningNonNull<FontFace>>&& aFontFaces) {
  FontFaceSetLoadEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mFontfaces.SwapElements(aFontFaces);
  RefPtr<FontFaceSetLoadEvent> event =
      FontFaceSetLoadEvent::Constructor(this, aType, init);
  (new AsyncEventDispatcher(this, event))->PostDOMEvent();
}

// nsIDOMEventListener

NS_IMETHODIMP
FontFaceSet::HandleEvent(Event* aEvent) {
  nsString type;
  aEvent->GetType(type);

  if (!type.EqualsLiteral("DOMContentLoaded")) {
    return NS_ERROR_FAILURE;
  }

  RemoveDOMContentLoadedListener();
  CheckLoadingFinished();

  return NS_OK;
}

/* static */
bool FontFaceSet::PrefEnabled() {
  static bool initialized = false;
  static bool enabled;
  if (!initialized) {
    initialized = true;
    Preferences::AddBoolVarCache(&enabled, FONT_LOADING_API_ENABLED_PREF);
  }
  return enabled;
}

// nsICSSLoaderObserver

NS_IMETHODIMP
FontFaceSet::StyleSheetLoaded(StyleSheet* aSheet, bool aWasDeferred,
                              nsresult aStatus) {
  CheckLoadingFinished();
  return NS_OK;
}

void FontFaceSet::FlushUserFontSet() {
  if (mDocument) {
    mDocument->FlushUserFontSet();
  }
}

void FontFaceSet::MarkUserFontSetDirty() {
  if (mDocument) {
    // Ensure we trigger at least a style flush, that will eventually flush the
    // user font set. Otherwise the font loads that that flush may cause could
    // never be triggered.
    if (PresShell* presShell = mDocument->GetPresShell()) {
      presShell->EnsureStyleFlush();
    }
    mDocument->MarkUserFontSetDirty();
  }
}

nsPresContext* FontFaceSet::GetPresContext() {
  if (!mDocument) {
    return nullptr;
  }

  return mDocument->GetPresContext();
}

void FontFaceSet::RefreshStandardFontLoadPrincipal() {
  MOZ_ASSERT(NS_IsMainThread());
  mStandardFontLoadPrincipal =
      new gfxFontSrcPrincipal(mDocument->NodePrincipal());
  mAllowedFontLoads.Clear();
  if (mUserFontSet) {
    mUserFontSet->IncrementGeneration(false);
  }
}

void FontFaceSet::CopyNonRuleFacesTo(FontFaceSet* aFontFaceSet) const {
  for (const FontFaceRecord& rec : mNonRuleFaces) {
    ErrorResult rv;
    RefPtr<FontFace> f = rec.mFontFace;
    aFontFaceSet->Add(*f, rv);
    MOZ_ASSERT(!rv.Failed());
  }
}

// -- FontFaceSet::UserFontSet ------------------------------------------------

/* virtual */
bool FontFaceSet::UserFontSet::IsFontLoadAllowed(const gfxFontFaceSrc& aSrc) {
  return mFontFaceSet && mFontFaceSet->IsFontLoadAllowed(aSrc);
}

/* virtual */
void FontFaceSet::UserFontSet::DispatchFontLoadViolations(
    nsTArray<nsCOMPtr<nsIRunnable>>& aViolations) {
  if (mFontFaceSet) {
    mFontFaceSet->DispatchFontLoadViolations(aViolations);
  }
}

/* virtual */
nsresult FontFaceSet::UserFontSet::StartLoad(
    gfxUserFontEntry* aUserFontEntry, const gfxFontFaceSrc* aFontFaceSrc) {
  if (!mFontFaceSet) {
    return NS_ERROR_FAILURE;
  }
  return mFontFaceSet->StartLoad(aUserFontEntry, aFontFaceSrc);
}

void FontFaceSet::UserFontSet::RecordFontLoadDone(uint32_t aFontSize,
                                                  TimeStamp aDoneTime) {
  mDownloadCount++;
  mDownloadSize += aFontSize;
  Telemetry::Accumulate(Telemetry::WEBFONT_SIZE, aFontSize / 1024);

  if (!mFontFaceSet) {
    return;
  }

  TimeStamp navStart = mFontFaceSet->GetNavigationStartTimeStamp();
  TimeStamp zero;
  if (navStart != zero) {
    Telemetry::AccumulateTimeDelta(Telemetry::WEBFONT_DOWNLOAD_TIME_AFTER_START,
                                   navStart, aDoneTime);
  }
}

/* virtual */
nsresult FontFaceSet::UserFontSet::LogMessage(gfxUserFontEntry* aUserFontEntry,
                                              const char* aMessage,
                                              uint32_t aFlags,
                                              nsresult aStatus) {
  if (!mFontFaceSet) {
    return NS_ERROR_FAILURE;
  }
  return mFontFaceSet->LogMessage(aUserFontEntry, aMessage, aFlags, aStatus);
}

/* virtual */
nsresult FontFaceSet::UserFontSet::SyncLoadFontData(
    gfxUserFontEntry* aFontToLoad, const gfxFontFaceSrc* aFontFaceSrc,
    uint8_t*& aBuffer, uint32_t& aBufferLength) {
  if (!mFontFaceSet) {
    return NS_ERROR_FAILURE;
  }
  return mFontFaceSet->SyncLoadFontData(aFontToLoad, aFontFaceSrc, aBuffer,
                                        aBufferLength);
}

/* virtual */
bool FontFaceSet::UserFontSet::GetPrivateBrowsing() {
  return mFontFaceSet && mFontFaceSet->mPrivateBrowsing;
}

/* virtual */
void FontFaceSet::UserFontSet::DoRebuildUserFontSet() {
  if (!mFontFaceSet) {
    return;
  }
  mFontFaceSet->MarkUserFontSetDirty();
}

/* virtual */
already_AddRefed<gfxUserFontEntry>
FontFaceSet::UserFontSet::CreateUserFontEntry(
    const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, WeightRange aWeight,
    StretchRange aStretch, SlantStyleRange aStyle,
    const nsTArray<gfxFontFeature>& aFeatureSettings,
    const nsTArray<gfxFontVariation>& aVariationSettings,
    uint32_t aLanguageOverride, gfxCharacterMap* aUnicodeRanges,
    StyleFontDisplay aFontDisplay, RangeFlags aRangeFlags) {
  RefPtr<gfxUserFontEntry> entry = new FontFace::Entry(
      this, aFontFaceSrcList, aWeight, aStretch, aStyle, aFeatureSettings,
      aVariationSettings, aLanguageOverride, aUnicodeRanges, aFontDisplay,
      aRangeFlags);
  return entry.forget();
}

#undef LOG_ENABLED
#undef LOG
