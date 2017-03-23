/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontFaceSet.h"

#include "gfxFontConstants.h"
#include "mozilla/css/Declaration.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/FontFaceSetBinding.h"
#include "mozilla/dom/FontFaceSetIterator.h"
#include "mozilla/dom/FontFaceSetLoadEvent.h"
#include "mozilla/dom/FontFaceSetLoadEventBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Telemetry.h"
#include "nsAutoPtr.h"
#include "nsContentPolicyUtils.h"
#include "nsCSSParser.h"
#include "nsDeviceContext.h"
#include "nsFontFaceLoader.h"
#include "nsIConsoleService.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsINetworkPredictor.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsISupportsPriority.h"
#include "nsIWebNavigation.h"
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"
#include "nsIInputStream.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"
#include "nsStyleSet.h"
#include "nsUTF8Utils.h"
#include "nsDOMNavigationTiming.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

#define LOG(args) MOZ_LOG(gfxUserFontSet::GetUserFontsLog(), mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gfxUserFontSet::GetUserFontsLog(), \
                                  LogLevel::Debug)

#define FONT_LOADING_API_ENABLED_PREF "layout.css.font-loading-api.enabled"

NS_IMPL_CYCLE_COLLECTION_CLASS(FontFaceSet)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FontFaceSet, DOMEventTargetHelper)
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

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FontFaceSet, DOMEventTargetHelper)
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

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FontFaceSet)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

FontFaceSet::FontFaceSet(nsPIDOMWindowInner* aWindow, nsIDocument* aDocument)
  : DOMEventTargetHelper(aWindow)
  , mDocument(aDocument)
  , mResolveLazilyCreatedReadyPromise(false)
  , mStatus(FontFaceSetLoadStatus::Loaded)
  , mNonRuleFacesDirty(false)
  , mHasLoadingFontFaces(false)
  , mHasLoadingFontFacesIsDirty(false)
  , mDelayedLoadCheck(false)
{
  MOZ_ASSERT(mDocument, "We should get a valid document from the caller!");

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aWindow);

  // If the pref is not set, don't create the Promise (which the page wouldn't
  // be able to get to anyway) as it causes the window.FontFaceSet constructor
  // to be created.
  if (global && PrefEnabled()) {
    mResolveLazilyCreatedReadyPromise = true;
  }

  if (!mDocument->DidFireDOMContentLoaded()) {
    mDocument->AddSystemEventListener(NS_LITERAL_STRING("DOMContentLoaded"),
                                      this, false, false);
  }

  mDocument->CSSLoader()->AddObserver(this);

  mUserFontSet = new UserFontSet(this);
}

FontFaceSet::~FontFaceSet()
{
  Disconnect();
  for (auto it = mLoaders.Iter(); !it.Done(); it.Next()) {
    it.Get()->GetKey()->Cancel();
  }
}

JSObject*
FontFaceSet::WrapObject(JSContext* aContext, JS::Handle<JSObject*> aGivenProto)
{
  return FontFaceSetBinding::Wrap(aContext, this, aGivenProto);
}

void
FontFaceSet::Disconnect()
{
  RemoveDOMContentLoadedListener();

  if (mDocument && mDocument->CSSLoader()) {
    // We're null checking CSSLoader() since FontFaceSet::Disconnect() might be
    // being called during unlink, at which time the loader amy already have
    // been unlinked from the document.
    mDocument->CSSLoader()->RemoveObserver(this);
  }
}

void
FontFaceSet::RemoveDOMContentLoadedListener()
{
  if (mDocument) {
    mDocument->RemoveSystemEventListener(NS_LITERAL_STRING("DOMContentLoaded"),
                                         this, false);
  }
}

void
FontFaceSet::ParseFontShorthandForMatching(
                            const nsAString& aFont,
                            RefPtr<FontFamilyListRefCnt>& aFamilyList,
                            uint32_t& aWeight,
                            int32_t& aStretch,
                            uint8_t& aStyle,
                            ErrorResult& aRv)
{
  // Parse aFont as a 'font' property value.
  RefPtr<Declaration> declaration = new Declaration;
  declaration->InitializeEmpty();

  bool changed = false;
  nsCSSParser parser;
  parser.ParseProperty(eCSSProperty_font,
                       aFont,
                       mDocument->GetDocumentURI(),
                       mDocument->GetDocumentURI(),
                       mDocument->NodePrincipal(),
                       declaration,
                       &changed,
                       /* aIsImportant */ false);

  // All of the properties we are interested in should have been set at once.
  MOZ_ASSERT(changed == (declaration->HasProperty(eCSSProperty_font_family) &&
                         declaration->HasProperty(eCSSProperty_font_style) &&
                         declaration->HasProperty(eCSSProperty_font_weight) &&
                         declaration->HasProperty(eCSSProperty_font_stretch)));

  if (!changed) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  nsCSSCompressedDataBlock* data = declaration->GetNormalBlock();
  MOZ_ASSERT(!declaration->GetImportantBlock());

  const nsCSSValue* family = data->ValueFor(eCSSProperty_font_family);
  if (family->GetUnit() != eCSSUnit_FontFamilyList) {
    // We got inherit, initial, unset, a system font, or a token stream.
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  aFamilyList =
    static_cast<FontFamilyListRefCnt*>(family->GetFontFamilyListValue());

  int32_t weight = data->ValueFor(eCSSProperty_font_weight)->GetIntValue();

  // Resolve relative font weights against the initial of font-weight
  // (normal, which is equivalent to 400).
  if (weight == NS_STYLE_FONT_WEIGHT_BOLDER) {
    weight = NS_FONT_WEIGHT_BOLD;
  } else if (weight == NS_STYLE_FONT_WEIGHT_LIGHTER) {
    weight = NS_FONT_WEIGHT_THIN;
  }

  aWeight = weight;

  aStretch = data->ValueFor(eCSSProperty_font_stretch)->GetIntValue();
  aStyle = data->ValueFor(eCSSProperty_font_style)->GetIntValue();
}

static bool
HasAnyCharacterInUnicodeRange(gfxUserFontEntry* aEntry,
                              const nsAString& aInput)
{
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

void
FontFaceSet::FindMatchingFontFaces(const nsAString& aFont,
                                   const nsAString& aText,
                                   nsTArray<FontFace*>& aFontFaces,
                                   ErrorResult& aRv)
{
  RefPtr<FontFamilyListRefCnt> familyList;
  uint32_t weight;
  int32_t stretch;
  uint8_t italicStyle;
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

  for (const FontFamilyName& fontFamilyName : familyList->GetFontlist()) {
    RefPtr<gfxFontFamily> family =
      mUserFontSet->LookupFamily(fontFamilyName.mName);

    if (!family) {
      continue;
    }

    AutoTArray<gfxFontEntry*,4> entries;
    bool needsBold;
    family->FindAllFontsForStyle(style, entries, needsBold);

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

TimeStamp
FontFaceSet::GetNavigationStartTimeStamp()
{
  TimeStamp navStart;
  RefPtr<nsDOMNavigationTiming> timing(mDocument->GetNavigationTiming());
  if (timing) {
    navStart = timing->GetNavigationStartTimeStamp();
  }
  return navStart;
}

already_AddRefed<Promise>
FontFaceSet::Load(JSContext* aCx,
                  const nsAString& aFont,
                  const nsAString& aText,
                  ErrorResult& aRv)
{
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

  nsIGlobalObject* globalObject = GetParentObject();
  if (!globalObject) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JS::Rooted<JSObject*> jsGlobal(aCx, globalObject->GetGlobalJSObject());
  GlobalObject global(aCx, jsGlobal);

  RefPtr<Promise> result = Promise::All(global, promises, aRv);
  return result.forget();
}

bool
FontFaceSet::Check(const nsAString& aFont,
                   const nsAString& aText,
                   ErrorResult& aRv)
{
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

Promise*
FontFaceSet::GetReady(ErrorResult& aRv)
{
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

  FlushUserFontSet();
  return mReady;
}

FontFaceSetLoadStatus
FontFaceSet::Status()
{
  FlushUserFontSet();
  return mStatus;
}

#ifdef DEBUG
bool
FontFaceSet::HasRuleFontFace(FontFace* aFontFace)
{
  for (size_t i = 0; i < mRuleFaces.Length(); i++) {
    if (mRuleFaces[i].mFontFace == aFontFace) {
      return true;
    }
  }
  return false;
}
#endif

FontFaceSet*
FontFaceSet::Add(FontFace& aFontFace, ErrorResult& aRv)
{
  FlushUserFontSet();

  if (aFontFace.IsInFontFaceSet(this)) {
    return this;
  }

  if (aFontFace.HasRule()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_MODIFICATION_ERR);
    return nullptr;
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
  rec->mSheetType = SheetType::Unknown;  // unused for mNonRuleFaces
  rec->mLoadEventShouldFire =
    aFontFace.Status() == FontFaceLoadStatus::Unloaded ||
    aFontFace.Status() == FontFaceLoadStatus::Loading;

  mNonRuleFacesDirty = true;
  RebuildUserFontSet();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingStarted();
  return this;
}

void
FontFaceSet::Clear()
{
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
  RebuildUserFontSet();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingFinished();
}

bool
FontFaceSet::Delete(FontFace& aFontFace)
{
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
  RebuildUserFontSet();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingFinished();
  return true;
}

bool
FontFaceSet::HasAvailableFontFace(FontFace* aFontFace)
{
  return aFontFace->IsInFontFaceSet(this);
}

bool
FontFaceSet::Has(FontFace& aFontFace)
{
  FlushUserFontSet();

  return HasAvailableFontFace(&aFontFace);
}

FontFace*
FontFaceSet::GetFontFaceAt(uint32_t aIndex)
{
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

uint32_t
FontFaceSet::Size()
{
  FlushUserFontSet();

  // Web IDL objects can only expose array index properties up to INT32_MAX.

  size_t total = mRuleFaces.Length() + mNonRuleFaces.Length();
  return std::min<size_t>(total, INT32_MAX);
}

already_AddRefed<FontFaceSetIterator>
FontFaceSet::Entries()
{
  RefPtr<FontFaceSetIterator> it = new FontFaceSetIterator(this, true);
  return it.forget();
}

already_AddRefed<FontFaceSetIterator>
FontFaceSet::Values()
{
  RefPtr<FontFaceSetIterator> it = new FontFaceSetIterator(this, false);
  return it.forget();
}

void
FontFaceSet::ForEach(JSContext* aCx,
                     FontFaceSetForEachCallback& aCallback,
                     JS::Handle<JS::Value> aThisArg,
                     ErrorResult& aRv)
{
  JS::Rooted<JS::Value> thisArg(aCx, aThisArg);
  for (size_t i = 0; i < Size(); i++) {
    FontFace* face = GetFontFaceAt(i);
    aCallback.Call(thisArg, *face, *face, *this, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
}

void
FontFaceSet::RemoveLoader(nsFontFaceLoader* aLoader)
{
  mLoaders.RemoveEntry(aLoader);
}

nsresult
FontFaceSet::StartLoad(gfxUserFontEntry* aUserFontEntry,
                       const gfxFontFaceSrc* aFontFaceSrc)
{
  nsresult rv;

  nsCOMPtr<nsIStreamLoader> streamLoader;
  nsCOMPtr<nsILoadGroup> loadGroup(mDocument->GetDocumentLoadGroup());

  nsCOMPtr<nsIChannel> channel;
  // Note we are calling NS_NewChannelWithTriggeringPrincipal() with both a
  // node and a principal.  This is because the document where the font is
  // being loaded might have a different origin from the principal of the
  // stylesheet that initiated the font load.
  rv = NS_NewChannelWithTriggeringPrincipal(getter_AddRefs(channel),
                                            aFontFaceSrc->mURI,
                                            mDocument,
                                            aUserFontEntry->GetPrincipal(),
                                            nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS,
                                            nsIContentPolicy::TYPE_FONT,
                                            loadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsFontFaceLoader> fontLoader =
    new nsFontFaceLoader(aUserFontEntry, aFontFaceSrc->mURI, this, channel);

  if (LOG_ENABLED()) {
    LOG(("userfonts (%p) download start - font uri: (%s) "
         "referrer uri: (%s)\n",
         fontLoader.get(), aFontFaceSrc->mURI->GetSpecOrDefault().get(),
         aFontFaceSrc->mReferrer
         ? aFontFaceSrc->mReferrer->GetSpecOrDefault().get()
         : ""));
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    rv = httpChannel->SetReferrerWithPolicy(aFontFaceSrc->mReferrer,
                                            mDocument->GetReferrerPolicy());
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString accept("application/font-woff;q=0.9,*/*;q=0.8");
    if (Preferences::GetBool(GFX_PREF_WOFF2_ENABLED)) {
      accept.Insert(NS_LITERAL_CSTRING("application/font-woff2;q=1.0,"), 0);
    }
    rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                       accept, false);
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

  rv = NS_NewStreamLoader(getter_AddRefs(streamLoader), fontLoader);
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::net::PredictorLearn(aFontFaceSrc->mURI, mDocument->GetDocumentURI(),
                               nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE,
                               loadGroup);

  rv = channel->AsyncOpen2(streamLoader);
  if (NS_FAILED(rv)) {
    fontLoader->DropChannel();  // explicitly need to break ref cycle
  }

  if (NS_SUCCEEDED(rv)) {
    mLoaders.PutEntry(fontLoader);
    fontLoader->StartedLoading(streamLoader);
    aUserFontEntry->SetLoader(fontLoader); // let the font entry remember the
                                           // loader, in case we need to cancel it
  }

  return rv;
}

bool
FontFaceSet::UpdateRules(const nsTArray<nsFontFaceRuleContainer>& aRules)
{
  MOZ_ASSERT(mUserFontSet);

  // If there was a change to the mNonRuleFaces array, then there could
  // have been a modification to the user font set.
  bool modified = mNonRuleFacesDirty;
  mNonRuleFacesDirty = false;

  // reuse existing FontFace objects mapped to rules already
  nsDataHashtable<nsPtrHashKey<nsCSSFontFaceRule>, FontFace*> ruleFaceMap;
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
  nsTHashtable<nsPtrHashKey<nsCSSFontFaceRule>> handledRules;

  for (size_t i = 0, i_end = aRules.Length(); i < i_end; ++i) {
    // Insert each FontFace objects for each rule into our list, migrating old
    // font entries if possible rather than creating new ones; set  modified  to
    // true if we detect that rule ordering has changed, or if a new entry is
    // created.
    if (handledRules.Contains(aRules[i].mRule)) {
      continue;
    }
    nsCSSFontFaceRule* rule = aRules[i].mRule;
    RefPtr<FontFace> f = ruleFaceMap.Get(rule);
    if (!f.get()) {
      f = FontFace::CreateForRule(GetParentObject(), this, rule);
    }
    InsertRuleFontFace(f, aRules[i].mSheetType, oldRecords, modified);
    handledRules.PutEntry(aRules[i].mRule);
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
         mUserFontSet.get(),
         (modified ? "modified" : "not modified"),
         (int)(mRuleFaces.Length())));
  }

  return modified;
}

static bool
HasLocalSrc(const nsCSSValue::Array *aSrcArr)
{
  size_t numSrc = aSrcArr->Count();
  for (size_t i = 0; i < numSrc; i++) {
    if (aSrcArr->Item(i).GetUnit() == eCSSUnit_Local_Font) {
      return true;
    }
  }
  return false;
}

void
FontFaceSet::IncrementGeneration(bool aIsRebuild)
{
  MOZ_ASSERT(mUserFontSet);
  mUserFontSet->IncrementGeneration(aIsRebuild);
}

void
FontFaceSet::InsertNonRuleFontFace(FontFace* aFontFace,
                                   bool& aFontSetModified)
{
  nsAutoString fontfamily;
  if (!aFontFace->GetFamilyName(fontfamily)) {
    // If there is no family name, this rule cannot contribute a
    // usable font, so there is no point in processing it further.
    return;
  }

  // Just create a new font entry if we haven't got one already.
  if (!aFontFace->GetUserFontEntry()) {
    // XXX Should we be checking mUserFontSet->mLocalRulesUsed like
    // InsertRuleFontFace does?
    RefPtr<gfxUserFontEntry> entry =
      FindOrCreateUserFontEntryFromFontFace(fontfamily, aFontFace,
                                            SheetType::Doc);
    if (!entry) {
      return;
    }
    aFontFace->SetUserFontEntry(entry);
  }

  aFontSetModified = true;
  mUserFontSet->AddUserFontEntry(fontfamily, aFontFace->GetUserFontEntry());
}

void
FontFaceSet::InsertRuleFontFace(FontFace* aFontFace, SheetType aSheetType,
                                nsTArray<FontFaceRecord>& aOldRecords,
                                bool& aFontSetModified)
{
  nsAutoString fontfamily;
  if (!aFontFace->GetFamilyName(fontfamily)) {
    // If there is no family name, this rule cannot contribute a
    // usable font, so there is no point in processing it further.
    return;
  }

  bool remove = false;
  size_t removeIndex;

  // This is a rule backed FontFace.  First, we check in aOldRecords; if
  // the FontFace for the rule exists there, just move it to the new record
  // list, and put the entry into the appropriate family.
  for (size_t i = 0; i < aOldRecords.Length(); ++i) {
    FontFaceRecord& rec = aOldRecords[i];

    if (rec.mFontFace == aFontFace &&
        rec.mSheetType == aSheetType) {

      // if local rules were used, don't use the old font entry
      // for rules containing src local usage
      if (mUserFontSet->mLocalRulesUsed &&
          mUserFontSet->mRebuildLocalRules) {
        nsCSSValue val;
        aFontFace->GetDesc(eCSSFontDesc_Src, val);
        nsCSSUnit unit = val.GetUnit();
        if (unit == eCSSUnit_Array && HasLocalSrc(val.GetArrayValue())) {
          // Remove the old record, but wait to see if we successfully create a
          // new user font entry below.
          remove = true;
          removeIndex = i;
          break;
        }
      }

      gfxUserFontEntry* entry = rec.mFontFace->GetUserFontEntry();
      MOZ_ASSERT(entry, "FontFace should have a gfxUserFontEntry by now");

      mUserFontSet->AddUserFontEntry(fontfamily, entry);

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
    FindOrCreateUserFontEntryFromFontFace(fontfamily, aFontFace, aSheetType);

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
  rec.mSheetType = aSheetType;
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
  mUserFontSet->AddUserFontEntry(fontfamily, entry);
}

already_AddRefed<gfxUserFontEntry>
FontFaceSet::FindOrCreateUserFontEntryFromFontFace(FontFace* aFontFace)
{
  nsAutoString fontfamily;
  if (!aFontFace->GetFamilyName(fontfamily)) {
    // If there is no family name, this rule cannot contribute a
    // usable font, so there is no point in processing it further.
    return nullptr;
  }

  return FindOrCreateUserFontEntryFromFontFace(fontfamily, aFontFace,
                                               SheetType::Doc);
}

already_AddRefed<gfxUserFontEntry>
FontFaceSet::FindOrCreateUserFontEntryFromFontFace(const nsAString& aFamilyName,
                                                   FontFace* aFontFace,
                                                   SheetType aSheetType)
{
  nsCSSValue val;
  nsCSSUnit unit;

  uint32_t weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  int32_t stretch = NS_STYLE_FONT_STRETCH_NORMAL;
  uint8_t italicStyle = NS_STYLE_FONT_STYLE_NORMAL;
  uint32_t languageOverride = NO_FONT_LANGUAGE_OVERRIDE;
  uint8_t fontDisplay = NS_FONT_DISPLAY_AUTO;

  // set up weight
  aFontFace->GetDesc(eCSSFontDesc_Weight, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Integer || unit == eCSSUnit_Enumerated) {
    weight = val.GetIntValue();
    if (weight == 0) {
      weight = NS_STYLE_FONT_WEIGHT_NORMAL;
    }
  } else if (unit == eCSSUnit_Normal) {
    weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face weight has unexpected unit");
  }

  // set up stretch
  aFontFace->GetDesc(eCSSFontDesc_Stretch, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Enumerated) {
    stretch = val.GetIntValue();
  } else if (unit == eCSSUnit_Normal) {
    stretch = NS_STYLE_FONT_STRETCH_NORMAL;
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face stretch has unexpected unit");
  }

  // set up font style
  aFontFace->GetDesc(eCSSFontDesc_Style, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Enumerated) {
    italicStyle = val.GetIntValue();
  } else if (unit == eCSSUnit_Normal) {
    italicStyle = NS_STYLE_FONT_STYLE_NORMAL;
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face style has unexpected unit");
  }

  // set up font display
  aFontFace->GetDesc(eCSSFontDesc_Display, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Enumerated) {
    fontDisplay = val.GetIntValue();
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face style has unexpected unit");
  }

  // set up font features
  nsTArray<gfxFontFeature> featureSettings;
  aFontFace->GetDesc(eCSSFontDesc_FontFeatureSettings, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Normal) {
    // empty list of features
  } else if (unit == eCSSUnit_PairList || unit == eCSSUnit_PairListDep) {
    nsRuleNode::ComputeFontFeatures(val.GetPairListValue(), featureSettings);
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face font-feature-settings has unexpected unit");
  }

  // set up font language override
  aFontFace->GetDesc(eCSSFontDesc_FontLanguageOverride, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Normal) {
    // empty feature string
  } else if (unit == eCSSUnit_String) {
    nsString stringValue;
    val.GetStringValue(stringValue);
    languageOverride = nsRuleNode::ParseFontLanguageOverride(stringValue);
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face font-language-override has unexpected unit");
  }

  // set up unicode-range
  nsAutoPtr<gfxCharacterMap> unicodeRanges;
  aFontFace->GetDesc(eCSSFontDesc_UnicodeRange, val);
  unit = val.GetUnit();
  if (unit == eCSSUnit_Array) {
    unicodeRanges = new gfxCharacterMap();
    const nsCSSValue::Array& sources = *val.GetArrayValue();
    MOZ_ASSERT(sources.Count() % 2 == 0,
               "odd number of entries in a unicode-range: array");

    for (uint32_t i = 0; i < sources.Count(); i += 2) {
      uint32_t min = sources[i].GetIntValue();
      uint32_t max = sources[i+1].GetIntValue();
      unicodeRanges->SetRange(min, max);
    }
  }

  // set up src array
  nsTArray<gfxFontFaceSrc> srcArray;

  if (aFontFace->HasFontData()) {
    gfxFontFaceSrc* face = srcArray.AppendElement();
    if (!face)
      return nullptr;

    face->mSourceType = gfxFontFaceSrc::eSourceType_Buffer;
    face->mBuffer = aFontFace->CreateBufferSource();
  } else {
    aFontFace->GetDesc(eCSSFontDesc_Src, val);
    unit = val.GetUnit();
    if (unit == eCSSUnit_Array) {
      nsCSSValue::Array* srcArr = val.GetArrayValue();
      size_t numSrc = srcArr->Count();

      for (size_t i = 0; i < numSrc; i++) {
        val = srcArr->Item(i);
        unit = val.GetUnit();
        gfxFontFaceSrc* face = srcArray.AppendElements(1);
        if (!face)
          return nullptr;

        switch (unit) {

        case eCSSUnit_Local_Font:
          val.GetStringValue(face->mLocalName);
          face->mSourceType = gfxFontFaceSrc::eSourceType_Local;
          face->mURI = nullptr;
          face->mFormatFlags = 0;
          break;
        case eCSSUnit_URL:
          face->mSourceType = gfxFontFaceSrc::eSourceType_URL;
          face->mURI = val.GetURLValue();
          face->mReferrer = val.GetURLStructValue()->mReferrer;
          face->mReferrerPolicy = mDocument->GetReferrerPolicy();
          face->mOriginPrincipal = val.GetURLStructValue()->mOriginPrincipal;
          NS_ASSERTION(face->mOriginPrincipal, "null origin principal in @font-face rule");

          // agent and user stylesheets are treated slightly differently,
          // the same-site origin check and access control headers are
          // enforced against the sheet principal rather than the document
          // principal to allow user stylesheets to include @font-face rules
          face->mUseOriginPrincipal = (aSheetType == SheetType::User ||
                                       aSheetType == SheetType::Agent);

          face->mLocalName.Truncate();
          face->mFormatFlags = 0;
          while (i + 1 < numSrc && (val = srcArr->Item(i+1),
                   val.GetUnit() == eCSSUnit_Font_Format)) {
            nsDependentString valueString(val.GetStringBufferValue());
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
            } else {
              // unknown format specified, mark to distinguish from the
              // case where no format hints are specified
              face->mFormatFlags |= gfxUserFontSet::FLAG_FORMAT_UNKNOWN;
            }
            i++;
          }
          if (!face->mURI) {
            // if URI not valid, omit from src array
            srcArray.RemoveElementAt(srcArray.Length() - 1);
            NS_WARNING("null url in @font-face rule");
            continue;
          }
          break;
        default:
          NS_ASSERTION(unit == eCSSUnit_Local_Font || unit == eCSSUnit_URL,
                       "strange unit type in font-face src array");
          break;
        }
       }
    } else {
      NS_ASSERTION(unit == eCSSUnit_Null, "@font-face src has unexpected unit");
    }
  }

  if (srcArray.IsEmpty()) {
    return nullptr;
  }

  RefPtr<gfxUserFontEntry> entry =
    mUserFontSet->FindOrCreateUserFontEntry(aFamilyName, srcArray, weight,
                                            stretch, italicStyle,
                                            featureSettings,
                                            languageOverride,
                                            unicodeRanges, fontDisplay);
  return entry.forget();
}

nsCSSFontFaceRule*
FontFaceSet::FindRuleForEntry(gfxFontEntry* aFontEntry)
{
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

nsCSSFontFaceRule*
FontFaceSet::FindRuleForUserFontEntry(gfxUserFontEntry* aUserFontEntry)
{
  for (uint32_t i = 0; i < mRuleFaces.Length(); ++i) {
    FontFace* f = mRuleFaces[i].mFontFace;
    if (f->GetUserFontEntry() == aUserFontEntry) {
      return f->GetRule();
    }
  }
  return nullptr;
}

nsresult
FontFaceSet::LogMessage(gfxUserFontEntry* aUserFontEntry,
                        const char* aMessage,
                        uint32_t aFlags,
                        nsresult aStatus)
{
  nsCOMPtr<nsIConsoleService>
    console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString familyName;
  nsAutoCString fontURI;
  aUserFontEntry->GetFamilyNameAndURIForLogging(familyName, fontURI);

  char weightKeywordBuf[8]; // plenty to sprintf() a uint16_t
  const char* weightKeyword;
  const nsAFlatCString& weightKeywordString =
    nsCSSProps::ValueToKeyword(aUserFontEntry->Weight(),
                               nsCSSProps::kFontWeightKTable);
  if (weightKeywordString.Length() > 0) {
    weightKeyword = weightKeywordString.get();
  } else {
    SprintfLiteral(weightKeywordBuf, "%u", aUserFontEntry->Weight());
    weightKeyword = weightKeywordBuf;
  }

  nsPrintfCString message
       ("downloadable font: %s "
        "(font-family: \"%s\" style:%s weight:%s stretch:%s src index:%d)",
        aMessage,
        familyName.get(),
        aUserFontEntry->IsItalic() ? "italic" : "normal",
        weightKeyword,
        nsCSSProps::ValueToKeyword(aUserFontEntry->Stretch(),
                                   nsCSSProps::kFontStretchKTable).get(),
        aUserFontEntry->GetSrcIndex());

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
  nsCSSFontFaceRule* rule = FindRuleForUserFontEntry(aUserFontEntry);
  nsString href;
  nsString text;
  nsresult rv;
  uint32_t line = 0;
  uint32_t column = 0;
  if (rule) {
    rv = rule->GetCssText(text);
    NS_ENSURE_SUCCESS(rv, rv);
    StyleSheet* sheet = rule->GetStyleSheet();
    // if the style sheet is removed while the font is loading can be null
    if (sheet) {
      nsCString spec = sheet->GetSheetURI()->GetSpecOrDefault();
      CopyUTF8toUTF16(spec, href);
    } else {
      NS_WARNING("null parent stylesheet for @font-face rule");
      href.AssignLiteral("unknown");
    }
    line = rule->GetLineNumber();
    column = rule->GetColumnNumber();
  }

  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t innerWindowID = mDocument->InnerWindowID();
  rv = scriptError->InitWithWindowID(NS_ConvertUTF8toUTF16(message),
                                     href,         // file
                                     text,         // src line
                                     line,
                                     column,
                                     aFlags,       // flags
                                     "CSS Loader", // category (make separate?)
                                     innerWindowID);
  if (NS_SUCCEEDED(rv)) {
    console->LogMessage(scriptError);
  }

  return NS_OK;
}

nsresult
FontFaceSet::CheckFontLoad(const gfxFontFaceSrc* aFontFaceSrc,
                           nsIPrincipal** aPrincipal,
                           bool* aBypassCache)
{
  NS_ASSERTION(aFontFaceSrc &&
               aFontFaceSrc->mSourceType == gfxFontFaceSrc::eSourceType_URL,
               "bad font face url passed to fontloader");

  // check same-site origin

  NS_ASSERTION(aFontFaceSrc->mURI, "null font uri");
  if (!aFontFaceSrc->mURI)
    return NS_ERROR_FAILURE;

  // use document principal, original principal if flag set
  // this enables user stylesheets to load font files via
  // @font-face rules
  *aPrincipal = mDocument->NodePrincipal();

  NS_ASSERTION(aFontFaceSrc->mOriginPrincipal,
               "null origin principal in @font-face rule");
  if (aFontFaceSrc->mUseOriginPrincipal) {
    *aPrincipal = aFontFaceSrc->mOriginPrincipal;
  }

  *aBypassCache = false;

  nsCOMPtr<nsIDocShell> docShell = mDocument->GetDocShell();
  if (docShell) {
    uint32_t loadType;
    if (NS_SUCCEEDED(docShell->GetLoadType(&loadType))) {
      if ((loadType >> 16) & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE) {
        *aBypassCache = true;
      }
    }
    uint32_t flags;
    if (NS_SUCCEEDED(docShell->GetDefaultLoadFlags(&flags))) {
      if (flags & nsIRequest::LOAD_BYPASS_CACHE) {
        *aBypassCache = true;
      }
    }
  }

  return NS_OK;
}


// @arg aPrincipal: generally this is mDocument->NodePrincipal() but
// might also be the original principal which enables user stylesheets
// to load font files via @font-face rules.
bool
FontFaceSet::IsFontLoadAllowed(nsIURI* aFontLocation, nsIPrincipal* aPrincipal)
{
  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_FONT,
                                          aFontLocation,
                                          aPrincipal,
                                          mDocument,
                                          EmptyCString(), // mime type
                                          nullptr, // aExtra
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy(),
                                          nsContentUtils::GetSecurityManager());

  return NS_SUCCEEDED(rv) && NS_CP_ACCEPTED(shouldLoad);
}

nsresult
FontFaceSet::SyncLoadFontData(gfxUserFontEntry* aFontToLoad,
                              const gfxFontFaceSrc* aFontFaceSrc,
                              uint8_t*& aBuffer,
                              uint32_t& aBufferLength)
{
  nsresult rv;

  nsCOMPtr<nsIChannel> channel;
  // Note we are calling NS_NewChannelWithTriggeringPrincipal() with both a
  // node and a principal.  This is because the document where the font is
  // being loaded might have a different origin from the principal of the
  // stylesheet that initiated the font load.
  // Further, we only get here for data: loads, so it doesn't really matter
  // whether we use SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS or not, to be more
  // restrictive we use SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS.
  rv = NS_NewChannelWithTriggeringPrincipal(getter_AddRefs(channel),
                                            aFontFaceSrc->mURI,
                                            mDocument,
                                            aFontToLoad->GetPrincipal(),
                                            nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS,
                                            nsIContentPolicy::TYPE_FONT);

  NS_ENSURE_SUCCESS(rv, rv);

  // blocking stream is OK for data URIs
  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open2(getter_AddRefs(stream));
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
  aBuffer = static_cast<uint8_t*> (moz_xmalloc(sizeof(uint8_t) * aBufferLength));
  if (!aBuffer) {
    aBufferLength = 0;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t numRead, totalRead = 0;
  while (NS_SUCCEEDED(rv =
           stream->Read(reinterpret_cast<char*>(aBuffer + totalRead),
                        aBufferLength - totalRead, &numRead)) &&
         numRead != 0)
  {
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

bool
FontFaceSet::GetPrivateBrowsing()
{
  nsCOMPtr<nsILoadContext> loadContext = mDocument->GetLoadContext();
  return loadContext && loadContext->UsePrivateBrowsing();
}

void
FontFaceSet::OnFontFaceStatusChanged(FontFace* aFontFace)
{
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
      nsCOMPtr<nsIRunnable> checkTask =
        NewRunnableMethod(this, &FontFaceSet::CheckLoadingFinishedAfterDelay);
      mDocument->Dispatch("FontFaceSet::CheckLoadingFinishedAfterDelay",
                          TaskCategory::Other, checkTask.forget());
    }
  }
}

void
FontFaceSet::DidRefresh()
{
  CheckLoadingFinished();
}

void
FontFaceSet::CheckLoadingFinishedAfterDelay()
{
  mDelayedLoadCheck = false;
  CheckLoadingFinished();
}

void
FontFaceSet::CheckLoadingStarted()
{
  if (!HasLoadingFontFaces()) {
    return;
  }

  if (mStatus == FontFaceSetLoadStatus::Loading) {
    // We have already dispatched a loading event and replaced mReady
    // with a fresh, unresolved promise.
    return;
  }

  mStatus = FontFaceSetLoadStatus::Loading;
  (new AsyncEventDispatcher(this, NS_LITERAL_STRING("loading"),
                            false))->PostDOMEvent();

  if (PrefEnabled()) {
    if (mReady) {
      if (GetParentObject()) {
        ErrorResult rv;
        mReady = Promise::Create(GetParentObject(), rv);
      }
    }
    if (!mReady) {
      mResolveLazilyCreatedReadyPromise = false;
    }
  }
}

void
FontFaceSet::UpdateHasLoadingFontFaces()
{
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

bool
FontFaceSet::HasLoadingFontFaces()
{
  if (mHasLoadingFontFacesIsDirty) {
    UpdateHasLoadingFontFaces();
  }
  return mHasLoadingFontFaces;
}

bool
FontFaceSet::MightHavePendingFontLoads()
{
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

void
FontFaceSet::CheckLoadingFinished()
{
  if (mDelayedLoadCheck) {
    // Wait until the runnable posted in OnFontFaceStatusChanged calls us.
    return;
  }

  if (mStatus == FontFaceSetLoadStatus::Loaded) {
    // We've already resolved mReady and dispatched the loadingdone/loadingerror
    // events.
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
                               Move(loaded));

  if (!failed.IsEmpty()) {
    DispatchLoadingFinishedEvent(NS_LITERAL_STRING("loadingerror"),
                                 Move(failed));
  }
}

void
FontFaceSet::DispatchLoadingFinishedEvent(
                                 const nsAString& aType,
                                 nsTArray<OwningNonNull<FontFace>>&& aFontFaces)
{
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
FontFaceSet::HandleEvent(nsIDOMEvent* aEvent)
{
  nsString type;
  aEvent->GetType(type);

  if (!type.EqualsLiteral("DOMContentLoaded")) {
    return NS_ERROR_FAILURE;
  }

  RemoveDOMContentLoadedListener();
  CheckLoadingFinished();

  return NS_OK;
}

/* static */ bool
FontFaceSet::PrefEnabled()
{
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
FontFaceSet::StyleSheetLoaded(StyleSheet* aSheet,
                              bool aWasAlternate,
                              nsresult aStatus)
{
  CheckLoadingFinished();
  return NS_OK;
}

void
FontFaceSet::FlushUserFontSet()
{
  if (mDocument) {
    mDocument->FlushUserFontSet();
  }
}

void
FontFaceSet::RebuildUserFontSet()
{
  if (mDocument) {
    mDocument->RebuildUserFontSet();
  }
}

nsPresContext*
FontFaceSet::GetPresContext()
{
  if (!mDocument) {
    return nullptr;
  }

  nsIPresShell* shell = mDocument->GetShell();
  return shell ? shell->GetPresContext() : nullptr;
}

// -- FontFaceSet::UserFontSet ------------------------------------------------

/* virtual */ nsresult
FontFaceSet::UserFontSet::CheckFontLoad(const gfxFontFaceSrc* aFontFaceSrc,
                                        nsIPrincipal** aPrincipal,
                                        bool* aBypassCache)
{
  if (!mFontFaceSet) {
    return NS_ERROR_FAILURE;
  }
  return mFontFaceSet->CheckFontLoad(aFontFaceSrc, aPrincipal, aBypassCache);
}

/* virtual */ bool
FontFaceSet::UserFontSet::IsFontLoadAllowed(nsIURI* aFontLocation,
                                            nsIPrincipal* aPrincipal)
{
  return mFontFaceSet &&
         mFontFaceSet->IsFontLoadAllowed(aFontLocation, aPrincipal);
}

/* virtual */ nsresult
FontFaceSet::UserFontSet::StartLoad(gfxUserFontEntry* aUserFontEntry,
                                    const gfxFontFaceSrc* aFontFaceSrc)
{
  if (!mFontFaceSet) {
    return NS_ERROR_FAILURE;
  }
  return mFontFaceSet->StartLoad(aUserFontEntry, aFontFaceSrc);
}

void
FontFaceSet::UserFontSet::RecordFontLoadDone(uint32_t aFontSize,
                                             TimeStamp aDoneTime)
{
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

/* virtual */ nsresult
FontFaceSet::UserFontSet::LogMessage(gfxUserFontEntry* aUserFontEntry,
                                     const char* aMessage,
                                     uint32_t aFlags,
                                     nsresult aStatus)
{
  if (!mFontFaceSet) {
    return NS_ERROR_FAILURE;
  }
  return mFontFaceSet->LogMessage(aUserFontEntry, aMessage, aFlags, aStatus);
}

/* virtual */ nsresult
FontFaceSet::UserFontSet::SyncLoadFontData(gfxUserFontEntry* aFontToLoad,
                                           const gfxFontFaceSrc* aFontFaceSrc,
                                           uint8_t*& aBuffer,
                                           uint32_t& aBufferLength)
{
  if (!mFontFaceSet) {
    return NS_ERROR_FAILURE;
  }
  return mFontFaceSet->SyncLoadFontData(aFontToLoad, aFontFaceSrc,
                                        aBuffer, aBufferLength);
}

/* virtual */ bool
FontFaceSet::UserFontSet::GetPrivateBrowsing()
{
  return mFontFaceSet && mFontFaceSet->GetPrivateBrowsing();
}

/* virtual */ void
FontFaceSet::UserFontSet::DoRebuildUserFontSet()
{
  if (!mFontFaceSet) {
    return;
  }
  mFontFaceSet->RebuildUserFontSet();
}

/* virtual */ already_AddRefed<gfxUserFontEntry>
FontFaceSet::UserFontSet::CreateUserFontEntry(
                               const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                               uint32_t aWeight,
                               int32_t aStretch,
                               uint8_t aStyle,
                               const nsTArray<gfxFontFeature>& aFeatureSettings,
                               uint32_t aLanguageOverride,
                               gfxSparseBitSet* aUnicodeRanges,
                               uint8_t aFontDisplay)
{
  RefPtr<gfxUserFontEntry> entry =
    new FontFace::Entry(this, aFontFaceSrcList, aWeight, aStretch, aStyle,
                        aFeatureSettings, aLanguageOverride, aUnicodeRanges,
                        aFontDisplay);
  return entry.forget();
}

#undef LOG_ENABLED
#undef LOG
