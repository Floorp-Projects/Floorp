/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontFaceSet.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include "mozilla/css/Loader.h"
#include "mozilla/dom/CSSFontFaceLoadEvent.h"
#include "mozilla/dom/CSSFontFaceLoadEventBinding.h"
#include "mozilla/dom/FontFaceSetBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Preferences.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsFontFaceLoader.h"
#include "nsIChannelPolicy.h"
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
#include "nsPresContext.h"
#include "nsPrintfCString.h"
#include "nsStyleSet.h"

using namespace mozilla;
using namespace mozilla::dom;

#ifdef PR_LOGGING
static PRLogModuleInfo*
GetFontFaceSetLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog)
    sLog = PR_NewLogModule("fontfaceset");
  return sLog;
}
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(GetFontFaceSetLog(), PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(GetFontFaceSetLog(), PR_LOG_DEBUG)

NS_IMPL_CYCLE_COLLECTION_CLASS(FontFaceSet)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FontFaceSet, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReady);
  for (size_t i = 0; i < tmp->mRuleFaces.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRuleFaces[i].mFontFace);
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNonRuleFaces);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FontFaceSet, DOMEventTargetHelper)
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReady);
  for (size_t i = 0; i < tmp->mRuleFaces.Length(); i++) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mRuleFaces[i].mFontFace);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNonRuleFaces);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(FontFaceSet, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FontFaceSet, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FontFaceSet)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

FontFaceSet::FontFaceSet(nsPIDOMWindow* aWindow, nsPresContext* aPresContext)
  : DOMEventTargetHelper(aWindow)
  , mPresContext(aPresContext)
  , mDocument(aPresContext->Document())
  , mStatus(FontFaceSetLoadStatus::Loaded)
  , mNonRuleFacesDirty(false)
  , mReadyIsResolved(true)
  , mDispatchedLoadingEvent(false)
  , mHasLoadingFontFaces(false)
  , mHasLoadingFontFacesIsDirty(false)
{
  MOZ_COUNT_CTOR(FontFaceSet);

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aWindow);

  if (global) {
    ErrorResult rv;
    mReady = Promise::Create(global, rv);
  }

  if (mReady) {
    mReady->MaybeResolve(this);
  }

  if (!mDocument->DidFireDOMContentLoaded()) {
    mDocument->AddSystemEventListener(NS_LITERAL_STRING("DOMContentLoaded"),
                                      this, false, false);
  }

  mDocument->CSSLoader()->AddObserver(this);
}

FontFaceSet::~FontFaceSet()
{
  MOZ_COUNT_DTOR(FontFaceSet);

  NS_ASSERTION(mLoaders.Count() == 0, "mLoaders should have been emptied");

  Disconnect();
}

JSObject*
FontFaceSet::WrapObject(JSContext* aContext)
{
  return FontFaceSetBinding::Wrap(aContext, this);
}

void
FontFaceSet::Disconnect()
{
  if (mDocument) {
    mDocument->RemoveSystemEventListener(NS_LITERAL_STRING("DOMContentLoaded"),
                                         this, false);
    // We're null checking CSSLoader() since FontFaceSet::Disconnect() might be
    // being called during unlink, at which time the loader amy already have
    // been unlinked from the document.
    if (mDocument->CSSLoader()) {
      mDocument->CSSLoader()->RemoveObserver(this);
    }
  }
}

FontFaceSet::UserFontSet*
FontFaceSet::EnsureUserFontSet(nsPresContext* aPresContext)
{
  if (!mUserFontSet) {
    mUserFontSet = new UserFontSet(this);
    mPresContext = aPresContext;
  }
  return mUserFontSet;
}

already_AddRefed<Promise>
FontFaceSet::Load(const nsAString& aFont,
                  const nsAString& aText,
                  ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

bool
FontFaceSet::Check(const nsAString& aFont,
                   const nsAString& aText,
                   ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return false;
}

Promise*
FontFaceSet::GetReady(ErrorResult& aRv)
{
  if (!mReady) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  mPresContext->FlushUserFontSet();
  return mReady;
}

FontFaceSetLoadStatus
FontFaceSet::Status()
{
  mPresContext->FlushUserFontSet();
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
  mPresContext->FlushUserFontSet();

  // We currently only support FontFace objects being in a single FontFaceSet,
  // and we also restrict the FontFaceSet to contain only FontFaces created
  // in the same window.

  if (aFontFace.GetFontFaceSet() != this) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  if (aFontFace.HasRule()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_MODIFICATION_ERR);
    return nullptr;
  }

  if (aFontFace.IsInFontFaceSet()) {
    return this;
  }

  bool removed = mUnavailableFaces.RemoveElement(&aFontFace);
  if (!removed) {
    MOZ_ASSERT(false, "should have found aFontFace in mUnavailableFaces");
    return this;
  }

  aFontFace.SetIsInFontFaceSet(true);

  MOZ_ASSERT(!mNonRuleFaces.Contains(&aFontFace),
             "FontFace should not occur in mNonRuleFaces twice");

  mNonRuleFaces.AppendElement(&aFontFace);

  mNonRuleFacesDirty = true;
  mPresContext->RebuildUserFontSet();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingStarted();
  return this;
}

void
FontFaceSet::Clear()
{
  mPresContext->FlushUserFontSet();

  if (mNonRuleFaces.IsEmpty()) {
    return;
  }

  for (size_t i = 0; i < mNonRuleFaces.Length(); i++) {
    FontFace* f = mNonRuleFaces[i];
    f->SetIsInFontFaceSet(false);

    MOZ_ASSERT(!mUnavailableFaces.Contains(f),
               "FontFace should not occur in mUnavailableFaces twice");

    mUnavailableFaces.AppendElement(f);
  }

  mNonRuleFaces.Clear();
  mNonRuleFacesDirty = true;
  mPresContext->RebuildUserFontSet();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingFinished();
}

bool
FontFaceSet::Delete(FontFace& aFontFace, ErrorResult& aRv)
{
  mPresContext->FlushUserFontSet();

  if (aFontFace.HasRule()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_MODIFICATION_ERR);
    return false;
  }

  if (!mNonRuleFaces.RemoveElement(&aFontFace)) {
    return false;
  }

  aFontFace.SetIsInFontFaceSet(false);

  MOZ_ASSERT(!mUnavailableFaces.Contains(&aFontFace),
             "FontFace should not occur in mUnavailableFaces twice");

  mUnavailableFaces.AppendElement(&aFontFace);

  mNonRuleFacesDirty = true;
  mPresContext->RebuildUserFontSet();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingFinished();
  return true;
}

bool
FontFaceSet::HasAvailableFontFace(FontFace* aFontFace)
{
  return aFontFace->GetFontFaceSet() == this &&
         aFontFace->IsInFontFaceSet();
}

bool
FontFaceSet::Has(FontFace& aFontFace)
{
  mPresContext->FlushUserFontSet();

  return HasAvailableFontFace(&aFontFace);
}

FontFace*
FontFaceSet::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  mPresContext->FlushUserFontSet();

  if (aIndex < mRuleFaces.Length()) {
    aFound = true;
    return mRuleFaces[aIndex].mFontFace;
  }

  aIndex -= mRuleFaces.Length();
  if (aIndex < mNonRuleFaces.Length()) {
    aFound = true;
    return mNonRuleFaces[aIndex];
  }

  aFound = false;
  return nullptr;
}

uint32_t
FontFaceSet::Length()
{
  mPresContext->FlushUserFontSet();

  // Web IDL objects can only expose array index properties up to INT32_MAX.

  size_t total = mRuleFaces.Length() + mNonRuleFaces.Length();
  return std::min<size_t>(total, INT32_MAX);
}

static PLDHashOperator DestroyIterator(nsPtrHashKey<nsFontFaceLoader>* aKey,
                                       void* aUserArg)
{
  aKey->GetKey()->Cancel();
  return PL_DHASH_REMOVE;
}

void
FontFaceSet::DisconnectFromRule(FontFace* aFontFace)
{
  nsCSSFontFaceRule* rule = aFontFace->GetRule();
  aFontFace->DisconnectFromRule();
  mRuleFaceMap.Remove(rule);
}

void
FontFaceSet::DestroyUserFontSet()
{
  Disconnect();
  mDocument = nullptr;
  mPresContext = nullptr;
  mLoaders.EnumerateEntries(DestroyIterator, nullptr);
  for (size_t i = 0; i < mRuleFaces.Length(); i++) {
    DisconnectFromRule(mRuleFaces[i].mFontFace);
    mRuleFaces[i].mFontFace->SetUserFontEntry(nullptr);
  }
  for (size_t i = 0; i < mNonRuleFaces.Length(); i++) {
    mNonRuleFaces[i]->SetUserFontEntry(nullptr);
  }
  for (size_t i = 0; i < mUnavailableFaces.Length(); i++) {
    mUnavailableFaces[i]->SetUserFontEntry(nullptr);
  }
  mRuleFaces.Clear();
  mNonRuleFaces.Clear();
  mUnavailableFaces.Clear();
  mReady = nullptr;
  mUserFontSet = nullptr;
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

  nsIPresShell* ps = mPresContext->PresShell();
  if (!ps)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStreamLoader> streamLoader;
  nsCOMPtr<nsILoadGroup> loadGroup(ps->GetDocument()->GetDocumentLoadGroup());

  nsCOMPtr<nsIChannel> channel;
  // get Content Security Policy from principal to pass into channel
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = aUserFontEntry->GetPrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);
  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_FONT);
  }
  // Note we are calling NS_NewChannelInternal() with both a node and a
  // principal.  This is because the document where the font is being loaded
  // might have a different origin from the principal of the stylesheet
  // that initiated the font load.
  rv = NS_NewChannelInternal(getter_AddRefs(channel),
                             aFontFaceSrc->mURI,
                             ps->GetDocument(),
                             aUserFontEntry->GetPrincipal(),
                             nsILoadInfo::SEC_NORMAL,
                             nsIContentPolicy::TYPE_FONT,
                             channelPolicy,
                             loadGroup);

  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsFontFaceLoader> fontLoader =
    new nsFontFaceLoader(aUserFontEntry, aFontFaceSrc->mURI, this, channel);

  if (!fontLoader)
    return NS_ERROR_OUT_OF_MEMORY;

#ifdef PR_LOGGING
  if (LOG_ENABLED()) {
    nsAutoCString fontURI, referrerURI;
    aFontFaceSrc->mURI->GetSpec(fontURI);
    if (aFontFaceSrc->mReferrer)
      aFontFaceSrc->mReferrer->GetSpec(referrerURI);
    LOG(("fontdownloader (%p) download start - font uri: (%s) "
         "referrer uri: (%s)\n",
         fontLoader.get(), fontURI.get(), referrerURI.get()));
  }
#endif

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel)
    httpChannel->SetReferrer(aFontFaceSrc->mReferrer);
  nsCOMPtr<nsISupportsPriority> priorityChannel(do_QueryInterface(channel));
  if (priorityChannel) {
    priorityChannel->AdjustPriority(nsISupportsPriority::PRIORITY_HIGH);
  }

  rv = NS_NewStreamLoader(getter_AddRefs(streamLoader), fontLoader);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDocument *document = ps->GetDocument();
  mozilla::net::PredictorLearn(aFontFaceSrc->mURI, document->GetDocumentURI(),
                               nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE,
                               loadGroup);

  bool inherits = false;
  rv = NS_URIChainHasFlags(aFontFaceSrc->mURI,
                           nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
                           &inherits);
  if (NS_SUCCEEDED(rv) && inherits) {
    // allow data, javascript, etc URI's
    rv = channel->AsyncOpen(streamLoader, nullptr);
  } else {
    nsRefPtr<nsCORSListenerProxy> listener =
      new nsCORSListenerProxy(streamLoader, aUserFontEntry->GetPrincipal(), false);
    rv = listener->Init(channel);
    if (NS_SUCCEEDED(rv)) {
      rv = channel->AsyncOpen(listener, nullptr);
    }
    if (NS_FAILED(rv)) {
      fontLoader->DropChannel();  // explicitly need to break ref cycle
    }
  }

  if (NS_SUCCEEDED(rv)) {
    mLoaders.PutEntry(fontLoader);
    fontLoader->StartedLoading(streamLoader);
    aUserFontEntry->SetLoader(fontLoader); // let the font entry remember the
                                           // loader, in case we need to cancel it
  }

  return rv;
}

static PLDHashOperator DetachFontEntries(const nsAString& aKey,
                                         nsRefPtr<gfxUserFontFamily>& aFamily,
                                         void* aUserArg)
{
  aFamily->DetachFontEntries();
  return PL_DHASH_NEXT;
}

static PLDHashOperator RemoveIfEmpty(const nsAString& aKey,
                                     nsRefPtr<gfxUserFontFamily>& aFamily,
                                     void* aUserArg)
{
  return aFamily->GetFontList().Length() ? PL_DHASH_NEXT : PL_DHASH_REMOVE;
}

bool
FontFaceSet::UpdateRules(const nsTArray<nsFontFaceRuleContainer>& aRules)
{
  MOZ_ASSERT(mUserFontSet);

  // If there was a change to the mNonRuleFaces array, then there could
  // have been a modification to the user font set.
  bool modified = mNonRuleFacesDirty;
  mNonRuleFacesDirty = false;

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
  mUserFontSet->mFontFamilies.Enumerate(DetachFontEntries, nullptr);

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
    InsertRuleFontFace(FontFaceForRule(aRules[i].mRule),
                       aRules[i].mSheetType, oldRecords,
                       modified);
    handledRules.PutEntry(aRules[i].mRule);
  }

  for (size_t i = 0, i_end = mNonRuleFaces.Length(); i < i_end; ++i) {
    // Do the same for the non rule backed FontFace objects.
    InsertNonRuleFontFace(mNonRuleFaces[i], modified);
  }

  // Remove any residual families that have no font entries (i.e., they were
  // not defined at all by the updated set of @font-face rules).
  mUserFontSet->mFontFamilies.Enumerate(RemoveIfEmpty, nullptr);

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
      nsRefPtr<FontFace> f = oldRecords[i].mFontFace;
      gfxUserFontEntry* userFontEntry = f->GetUserFontEntry();
      if (userFontEntry) {
        nsFontFaceLoader* loader = userFontEntry->GetLoader();
        if (loader) {
          loader->Cancel();
          RemoveLoader(loader);
        }
      }

      // Any left over FontFace objects should also cease being rule backed.
      MOZ_ASSERT(!mUnavailableFaces.Contains(f),
                 "FontFace should not occur in mUnavailableFaces twice");

      mUnavailableFaces.AppendElement(f);
      DisconnectFromRule(f);
    }
  }

  if (modified) {
    IncrementGeneration(true);
    mHasLoadingFontFacesIsDirty = true;
    CheckLoadingStarted();
    CheckLoadingFinished();
  }

  // local rules have been rebuilt, so clear the flag
  mUserFontSet->mLocalRulesUsed = false;

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
  if (!aFontFace->IsInitialized()) {
    // The FontFace is still waiting to be initialized, so don't create a
    // user font entry for it yet.  Once it has been initialized, it will
    // call OnFontFaceInitialized on us, which will end rebuild the user
    // font set and end up back in here.
    return;
  }

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
    nsRefPtr<gfxUserFontEntry> entry =
      FindOrCreateUserFontEntryFromFontFace(fontfamily, aFontFace,
                                            nsStyleSet::eDocSheet);
    if (!entry) {
      return;
    }
    aFontFace->SetUserFontEntry(entry);
  }

  aFontSetModified = true;
  mUserFontSet->AddUserFontEntry(fontfamily, aFontFace->GetUserFontEntry());
}

void
FontFaceSet::InsertRuleFontFace(FontFace* aFontFace, uint8_t aSheetType,
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
      if (mUserFontSet->mLocalRulesUsed) {
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
  nsRefPtr<gfxUserFontEntry> entry =
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
                                               nsStyleSet::eDocSheet);
}

already_AddRefed<gfxUserFontEntry>
FontFaceSet::FindOrCreateUserFontEntryFromFontFace(const nsAString& aFamilyName,
                                                   FontFace* aFontFace,
                                                   uint8_t aSheetType)
{
  nsCSSValue val;
  uint32_t unit;

  uint32_t weight = NS_STYLE_FONT_WEIGHT_NORMAL;
  int32_t stretch = NS_STYLE_FONT_STRETCH_NORMAL;
  uint32_t italicStyle = NS_STYLE_FONT_STYLE_NORMAL;
  uint32_t languageOverride = NO_FONT_LANGUAGE_OVERRIDE;

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
    languageOverride = gfxFontStyle::ParseFontLanguageOverride(stringValue);
  } else {
    NS_ASSERTION(unit == eCSSUnit_Null,
                 "@font-face font-language-override has unexpected unit");
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
          face->mOriginPrincipal = val.GetURLStructValue()->mOriginPrincipal;
          NS_ASSERTION(face->mOriginPrincipal, "null origin principal in @font-face rule");

          // agent and user stylesheets are treated slightly differently,
          // the same-site origin check and access control headers are
          // enforced against the sheet principal rather than the document
          // principal to allow user stylesheets to include @font-face rules
          face->mUseOriginPrincipal = (aSheetType == nsStyleSet::eUserSheet ||
                                       aSheetType == nsStyleSet::eAgentSheet);

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

  nsRefPtr<gfxUserFontEntry> entry =
    mUserFontSet->FindOrCreateUserFontEntry(aFamilyName, srcArray, weight,
                                            stretch, italicStyle,
                                            featureSettings,
                                            languageOverride,
                                            nullptr /* aUnicodeRanges */);
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

gfxUserFontEntry*
FontFaceSet::FindUserFontEntryForRule(nsCSSFontFaceRule* aRule)
{
  FontFace* f = mRuleFaceMap.Get(aRule);
  if (f) {
    return f->GetUserFontEntry();
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
    sprintf(weightKeywordBuf, "%u", aUserFontEntry->Weight());
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
  message.AppendLiteral("\nsource: ");
  message.Append(fontURI);

#ifdef PR_LOGGING
  if (PR_LOG_TEST(GetFontFaceSetLog(), PR_LOG_DEBUG)) {
    PR_LOG(GetFontFaceSetLog(), PR_LOG_DEBUG,
           ("userfonts (%p) %s", this, message.get()));
  }
#endif

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
    CSSStyleSheet* sheet = rule->GetStyleSheet();
    // if the style sheet is removed while the font is loading can be null
    if (sheet) {
      nsAutoCString spec;
      rv = sheet->GetSheetURI()->GetSpec(spec);
      NS_ENSURE_SUCCESS(rv, rv);
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

  uint64_t innerWindowID = GetPresContext()->Document()->InnerWindowID();
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
  // check same-site origin
  nsIPresShell* ps = mPresContext->PresShell();
  if (!ps)
    return NS_ERROR_FAILURE;

  NS_ASSERTION(aFontFaceSrc &&
               aFontFaceSrc->mSourceType == gfxFontFaceSrc::eSourceType_URL,
               "bad font face url passed to fontloader");
  NS_ASSERTION(aFontFaceSrc->mURI, "null font uri");
  if (!aFontFaceSrc->mURI)
    return NS_ERROR_FAILURE;

  // use document principal, original principal if flag set
  // this enables user stylesheets to load font files via
  // @font-face rules
  *aPrincipal = ps->GetDocument()->NodePrincipal();

  NS_ASSERTION(aFontFaceSrc->mOriginPrincipal,
               "null origin principal in @font-face rule");
  if (aFontFaceSrc->mUseOriginPrincipal) {
    *aPrincipal = aFontFaceSrc->mOriginPrincipal;
  }

  nsresult rv = nsFontFaceLoader::CheckLoadAllowed(*aPrincipal,
                                                   aFontFaceSrc->mURI,
                                                   ps->GetDocument());
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aBypassCache = false;

  nsCOMPtr<nsIDocShell> docShell = ps->GetDocument()->GetDocShell();
  if (docShell) {
    uint32_t loadType;
    if (NS_SUCCEEDED(docShell->GetLoadType(&loadType))) {
      if ((loadType >> 16) & nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE) {
        *aBypassCache = true;
      }
    }
  }

  return rv;
}

nsresult
FontFaceSet::SyncLoadFontData(gfxUserFontEntry* aFontToLoad,
                              const gfxFontFaceSrc* aFontFaceSrc,
                              uint8_t*& aBuffer,
                              uint32_t& aBufferLength)
{
  nsresult rv;

  nsCOMPtr<nsIChannel> channel;
  // get Content Security Policy from principal to pass into channel
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = aFontToLoad->GetPrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);
  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_FONT);
  }

  nsIPresShell* ps = mPresContext->PresShell();
  if (!ps) {
    return NS_ERROR_FAILURE;
  }
  // Note we are calling NS_NewChannelInternal() with both a node and a
  // principal.  This is because the document where the font is being loaded
  // might have a different origin from the principal of the stylesheet
  // that initiated the font load.
  rv = NS_NewChannelInternal(getter_AddRefs(channel),
                             aFontFaceSrc->mURI,
                             ps->GetDocument(),
                             aFontToLoad->GetPrincipal(),
                             nsILoadInfo::SEC_NORMAL,
                             nsIContentPolicy::TYPE_FONT,
                             channelPolicy);

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
  aBuffer = static_cast<uint8_t*> (NS_Alloc(sizeof(uint8_t) * aBufferLength));
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
    NS_Free(aBuffer);
    aBuffer = nullptr;
    aBufferLength = 0;
    return rv;
  }

  return NS_OK;
}

bool
FontFaceSet::GetPrivateBrowsing()
{
  nsIPresShell* ps = mPresContext->PresShell();
  if (!ps) {
    return false;
  }

  nsCOMPtr<nsILoadContext> loadContext = ps->GetDocument()->GetLoadContext();
  return loadContext && loadContext->UsePrivateBrowsing();
}

void
FontFaceSet::DoRebuildUserFontSet()
{
  if (!mPresContext) {
    // AFAICS, this can only happen if someone has already called Destroy() on
    // this font-set, which means it is in the process of being torn down --
    // so there's no point trying to update its rules.
    return;
  }

  mPresContext->RebuildUserFontSet();
}

FontFace*
FontFaceSet::FontFaceForRule(nsCSSFontFaceRule* aRule)
{
  MOZ_ASSERT(aRule);

  FontFace* f = mRuleFaceMap.Get(aRule);
  if (f) {
    MOZ_ASSERT(f->GetFontFaceSet() == this,
               "existing FontFace is from another FontFaceSet?");
    return f;
  }

  // We might be creating a FontFace object for an @font-face rule that we are
  // just about to create a user font entry for, so entry might be null.
  gfxUserFontEntry* entry = FindUserFontEntryForRule(aRule);
  nsRefPtr<FontFace> newFontFace =
    FontFace::CreateForRule(GetParentObject(), mPresContext, aRule, entry);
  MOZ_ASSERT(newFontFace->GetFontFaceSet() == this,
             "new FontFace is from another FontFaceSet?");
  mRuleFaceMap.Put(aRule, newFontFace);
  return newFontFace;
}

void
FontFaceSet::AddUnavailableFontFace(FontFace* aFontFace)
{
  MOZ_ASSERT(!aFontFace->HasRule());
  MOZ_ASSERT(!aFontFace->IsInFontFaceSet());
  MOZ_ASSERT(!mUnavailableFaces.Contains(aFontFace));

  mUnavailableFaces.AppendElement(aFontFace);
}

void
FontFaceSet::RemoveUnavailableFontFace(FontFace* aFontFace)
{
  MOZ_ASSERT(!aFontFace->HasRule());
  MOZ_ASSERT(!aFontFace->IsInFontFaceSet());

  // We might not actually find the FontFace in mUnavailableFaces, since we
  // might be shutting down the document and had DestroyUserFontSet called
  // on us, which clears out mUnavailableFaces.
  mUnavailableFaces.RemoveElement(aFontFace);

  MOZ_ASSERT(!mUnavailableFaces.Contains(aFontFace));
}

void
FontFaceSet::OnFontFaceInitialized(FontFace* aFontFace)
{
  MOZ_ASSERT(HasAvailableFontFace(aFontFace));
  MOZ_ASSERT(!aFontFace->HasRule());
  MOZ_ASSERT(aFontFace->IsInitialized());

  mPresContext->RebuildUserFontSet();
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
    CheckLoadingFinished();
  }
}

void
FontFaceSet::DidRefresh()
{
  CheckLoadingFinished();
}

void
FontFaceSet::CheckLoadingStarted()
{
  if (HasLoadingFontFaces() && !mDispatchedLoadingEvent) {
    mStatus = FontFaceSetLoadStatus::Loading;
    mDispatchedLoadingEvent = true;
    (new AsyncEventDispatcher(this, NS_LITERAL_STRING("loading"),
                              false))->RunDOMEventWhenSafe();
  }

  if (mReadyIsResolved) {
    nsRefPtr<Promise> ready;
    if (GetParentObject()) {
      ErrorResult rv;
      ready = Promise::Create(GetParentObject(), rv);
    }

    if (ready) {
      mReady.swap(ready);
      mReadyIsResolved = false;
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
    if (mNonRuleFaces[i]->Status() == FontFaceLoadStatus::Loading) {
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
  if (mPresContext && mPresContext->HasPendingRestyleOrReflow()) {
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
  if (mReadyIsResolved) {
    // We've already resolved mReady and dispatched the loadingdone/loadingerror
    // events.
    return;
  }

  if (MightHavePendingFontLoads()) {
    // We're not finished loading yet.
    return;
  }

  mStatus = FontFaceSetLoadStatus::Loaded;
  mDispatchedLoadingEvent = false;
  if (mReady) {
    mReady->MaybeResolve(this);
    mReadyIsResolved = true;
  }

  // Now dispatch the loadingdone/loadingerror events.
  nsTArray<FontFace*> loaded;
  nsTArray<FontFace*> failed;

  for (size_t i = 0; i < mRuleFaces.Length(); i++) {
    FontFace* f = mRuleFaces[i].mFontFace;
    if (f->Status() == FontFaceLoadStatus::Loaded) {
      loaded.AppendElement(f);
    } else if (f->Status() == FontFaceLoadStatus::Error) {
      failed.AppendElement(f);
    }
  }

  for (size_t i = 0; i < mNonRuleFaces.Length(); i++) {
    FontFace* f = mNonRuleFaces[i];
    if (f->Status() == FontFaceLoadStatus::Loaded) {
      loaded.AppendElement(f);
    } else if (f->Status() == FontFaceLoadStatus::Error) {
      failed.AppendElement(f);
    }
  }

  DispatchLoadingFinishedEvent(NS_LITERAL_STRING("loadingdone"), loaded);

  if (!failed.IsEmpty()) {
    DispatchLoadingFinishedEvent(NS_LITERAL_STRING("loadingerror"), failed);
  }
}

void
FontFaceSet::DispatchLoadingFinishedEvent(
                                 const nsAString& aType,
                                 const nsTArray<FontFace*>& aFontFaces)
{
  CSSFontFaceLoadEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  OwningNonNull<FontFace>* elements =
    init.mFontfaces.AppendElements(aFontFaces.Length());
  for (size_t i = 0; i < aFontFaces.Length(); i++) {
    elements[i] = aFontFaces[i];
  }
  nsRefPtr<CSSFontFaceLoadEvent> event =
    CSSFontFaceLoadEvent::Constructor(this, aType, init);
  (new AsyncEventDispatcher(this, event))->RunDOMEventWhenSafe();
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

  Disconnect();
  CheckLoadingFinished();

  return NS_OK;
}

// nsICSSLoaderObserver

NS_IMETHODIMP
FontFaceSet::StyleSheetLoaded(mozilla::CSSStyleSheet* aSheet,
                              bool aWasAlternate,
                              nsresult aStatus)
{
  CheckLoadingFinished();
  return NS_OK;
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

/* virtual */ nsresult
FontFaceSet::UserFontSet::StartLoad(gfxUserFontEntry* aUserFontEntry,
                                    const gfxFontFaceSrc* aFontFaceSrc)
{
  if (!mFontFaceSet) {
    return NS_ERROR_FAILURE;
  }
  return mFontFaceSet->StartLoad(aUserFontEntry, aFontFaceSrc);
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
  mFontFaceSet->DoRebuildUserFontSet();
}

/* virtual */ already_AddRefed<gfxUserFontEntry>
FontFaceSet::UserFontSet::CreateUserFontEntry(
                               const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                               uint32_t aWeight,
                               int32_t aStretch,
                               uint32_t aItalicStyle,
                               const nsTArray<gfxFontFeature>& aFeatureSettings,
                               uint32_t aLanguageOverride,
                               gfxSparseBitSet* aUnicodeRanges)
{
  nsRefPtr<gfxUserFontEntry> entry =
    new FontFace::Entry(this, aFontFaceSrcList, aWeight, aStretch, aItalicStyle,
                        aFeatureSettings, aLanguageOverride, aUnicodeRanges);
  return entry.forget();
}
