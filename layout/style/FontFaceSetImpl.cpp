/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontFaceSetImpl.h"

#include "gfxFontConstants.h"
#include "gfxFontSrcPrincipal.h"
#include "gfxFontSrcURI.h"
#include "gfxFontUtils.h"
#include "gfxPlatformFontList.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/CSSFontFaceRule.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FontFaceImpl.h"
#include "mozilla/dom/FontFaceSet.h"
#include "mozilla/dom/FontFaceSetBinding.h"
#include "mozilla/dom/FontFaceSetLoadEvent.h"
#include "mozilla/dom/FontFaceSetLoadEventBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/Telemetry.h"
#include "mozilla/LoadInfo.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDeviceContext.h"
#include "nsFontFaceLoader.h"
#include "nsIConsoleService.h"
#include "nsIContentPolicy.h"
#include "nsIDocShell.h"
#include "nsILoadContext.h"
#include "nsIPrincipal.h"
#include "nsIWebNavigation.h"
#include "nsNetUtil.h"
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

NS_IMPL_ISUPPORTS0(FontFaceSetImpl)

FontFaceSetImpl::FontFaceSetImpl(FontFaceSet* aOwner)
    : mMutex("mozilla::dom::FontFaceSetImpl"),
      mOwner(aOwner),
      mStatus(FontFaceSetLoadStatus::Loaded),
      mNonRuleFacesDirty(false),
      mHasLoadingFontFaces(false),
      mHasLoadingFontFacesIsDirty(false),
      mDelayedLoadCheck(false),
      mBypassCache(false),
      mPrivateBrowsing(false) {}

FontFaceSetImpl::~FontFaceSetImpl() {
  // Assert that we don't drop any FontFaceSet objects during a Servo traversal,
  // since PostTraversalTask objects can hold raw pointers to FontFaceSets.
  MOZ_ASSERT(!gfxFontUtils::IsInServoTraversal());

  Destroy();
}

void FontFaceSetImpl::DestroyLoaders() {
  mMutex.AssertCurrentThreadIn();
  if (mLoaders.IsEmpty()) {
    return;
  }
  if (NS_IsMainThread()) {
    for (const auto& key : mLoaders.Keys()) {
      key->Cancel();
    }
    mLoaders.Clear();
    return;
  }

  class DestroyLoadersRunnable final : public Runnable {
   public:
    explicit DestroyLoadersRunnable(FontFaceSetImpl* aFontFaceSet)
        : Runnable("FontFaceSetImpl::DestroyLoaders"),
          mFontFaceSet(aFontFaceSet) {}

   protected:
    ~DestroyLoadersRunnable() override = default;

    NS_IMETHOD Run() override {
      RecursiveMutexAutoLock lock(mFontFaceSet->mMutex);
      mFontFaceSet->DestroyLoaders();
      return NS_OK;
    }

    // We need to save a reference to the FontFaceSetImpl because the
    // loaders contain a non-owning reference to it.
    RefPtr<FontFaceSetImpl> mFontFaceSet;
  };

  auto runnable = MakeRefPtr<DestroyLoadersRunnable>(this);
  NS_DispatchToMainThread(runnable);
}

void FontFaceSetImpl::Destroy() {
  nsTArray<FontFaceRecord> nonRuleFaces;
  nsRefPtrHashtable<nsCStringHashKey, gfxUserFontFamily> fontFamilies;

  {
    RecursiveMutexAutoLock lock(mMutex);
    DestroyLoaders();
    nonRuleFaces = std::move(mNonRuleFaces);
    fontFamilies = std::move(mFontFamilies);
    mOwner = nullptr;
  }

  if (gfxPlatformFontList* fp = gfxPlatformFontList::PlatformFontList()) {
    fp->RemoveUserFontSet(this);
  }
}

void FontFaceSetImpl::ParseFontShorthandForMatching(
    const nsACString& aFont, StyleFontFamilyList& aFamilyList,
    FontWeight& aWeight, FontStretch& aStretch, FontSlantStyle& aStyle,
    ErrorResult& aRv) {
  RefPtr<URLExtraData> url = GetURLExtraData();
  if (!url) {
    aRv.ThrowInvalidStateError("Missing URLExtraData");
    return;
  }

  if (!ServoCSSParser::ParseFontShorthandForMatching(
          aFont, url, aFamilyList, aStyle, aStretch, aWeight)) {
    aRv.ThrowSyntaxError("Invalid font shorthand");
    return;
  }
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

void FontFaceSetImpl::FindMatchingFontFaces(const nsACString& aFont,
                                            const nsAString& aText,
                                            nsTArray<FontFace*>& aFontFaces,
                                            ErrorResult& aRv) {
  RecursiveMutexAutoLock lock(mMutex);

  StyleFontFamilyList familyList;
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

  // Set of FontFaces that we want to return.
  nsTHashSet<FontFace*> matchingFaces;

  for (const StyleSingleFontFamily& fontFamilyName : familyList.list.AsSpan()) {
    if (!fontFamilyName.IsFamilyName()) {
      continue;
    }

    const auto& name = fontFamilyName.AsFamilyName();
    RefPtr<gfxFontFamily> family =
        LookupFamily(nsAtomCString(name.name.AsAtom()));

    if (!family) {
      continue;
    }

    AutoTArray<gfxFontEntry*, 4> entries;
    family->FindAllFontsForStyle(style, entries);

    for (gfxFontEntry* e : entries) {
      FontFaceImpl::Entry* entry = static_cast<FontFaceImpl::Entry*>(e);
      if (HasAnyCharacterInUnicodeRange(entry, aText)) {
        entry->FindFontFaceOwners(matchingFaces);
      }
    }
  }

  if (matchingFaces.IsEmpty()) {
    return;
  }

  // Add all FontFaces in matchingFaces to aFontFaces, in the order
  // they appear in the FontFaceSet.
  FindMatchingFontFaces(matchingFaces, aFontFaces);
}

void FontFaceSetImpl::FindMatchingFontFaces(
    const nsTHashSet<FontFace*>& aMatchingFaces,
    nsTArray<FontFace*>& aFontFaces) {
  RecursiveMutexAutoLock lock(mMutex);
  for (FontFaceRecord& record : mNonRuleFaces) {
    FontFace* owner = record.mFontFace->GetOwner();
    if (owner && aMatchingFaces.Contains(owner)) {
      aFontFaces.AppendElement(owner);
    }
  }
}

bool FontFaceSetImpl::ReadyPromiseIsPending() const {
  RecursiveMutexAutoLock lock(mMutex);
  return mOwner && mOwner->ReadyPromiseIsPending();
}

FontFaceSetLoadStatus FontFaceSetImpl::Status() {
  RecursiveMutexAutoLock lock(mMutex);
  FlushUserFontSet();
  return mStatus;
}

bool FontFaceSetImpl::Add(FontFaceImpl* aFontFace, ErrorResult& aRv) {
  RecursiveMutexAutoLock lock(mMutex);
  FlushUserFontSet();

  if (aFontFace->IsInFontFaceSet(this)) {
    return false;
  }

  if (aFontFace->HasRule()) {
    aRv.ThrowInvalidModificationError(
        "Can't add face to FontFaceSet that comes from an @font-face rule");
    return false;
  }

  aFontFace->AddFontFaceSet(this);

#ifdef DEBUG
  for (const FontFaceRecord& rec : mNonRuleFaces) {
    MOZ_ASSERT(rec.mFontFace != aFontFace,
               "FontFace should not occur in mNonRuleFaces twice");
  }
#endif

  FontFaceRecord* rec = mNonRuleFaces.AppendElement();
  rec->mFontFace = aFontFace;
  rec->mOrigin = Nothing();

  mNonRuleFacesDirty = true;
  MarkUserFontSetDirty();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingStarted();
  return true;
}

void FontFaceSetImpl::Clear() {
  RecursiveMutexAutoLock lock(mMutex);
  FlushUserFontSet();

  if (mNonRuleFaces.IsEmpty()) {
    return;
  }

  for (size_t i = 0; i < mNonRuleFaces.Length(); i++) {
    FontFaceImpl* f = mNonRuleFaces[i].mFontFace;
    f->RemoveFontFaceSet(this);
  }

  mNonRuleFaces.Clear();
  mNonRuleFacesDirty = true;
  MarkUserFontSetDirty();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingFinished();
}

bool FontFaceSetImpl::Delete(FontFaceImpl* aFontFace) {
  RecursiveMutexAutoLock lock(mMutex);
  FlushUserFontSet();

  if (aFontFace->HasRule()) {
    return false;
  }

  bool removed = false;
  for (size_t i = 0; i < mNonRuleFaces.Length(); i++) {
    if (mNonRuleFaces[i].mFontFace == aFontFace) {
      mNonRuleFaces.RemoveElementAt(i);
      removed = true;
      break;
    }
  }
  if (!removed) {
    return false;
  }

  aFontFace->RemoveFontFaceSet(this);

  mNonRuleFacesDirty = true;
  MarkUserFontSetDirty();
  mHasLoadingFontFacesIsDirty = true;
  CheckLoadingFinished();
  return true;
}

bool FontFaceSetImpl::HasAvailableFontFace(FontFaceImpl* aFontFace) {
  return aFontFace->IsInFontFaceSet(this);
}

void FontFaceSetImpl::RemoveLoader(nsFontFaceLoader* aLoader) {
  RecursiveMutexAutoLock lock(mMutex);
  mLoaders.RemoveEntry(aLoader);
}

void FontFaceSetImpl::InsertNonRuleFontFace(FontFaceImpl* aFontFace) {
  gfxUserFontAttributes attr;
  if (!aFontFace->GetAttributes(attr)) {
    // If there is no family name, this rule cannot contribute a
    // usable font, so there is no point in processing it further.
    return;
  }

  nsAutoCString family(attr.mFamilyName);

  // Just create a new font entry if we haven't got one already.
  if (!aFontFace->GetUserFontEntry()) {
    // XXX Should we be checking mLocalRulesUsed like InsertRuleFontFace does?
    RefPtr<gfxUserFontEntry> entry = FindOrCreateUserFontEntryFromFontFace(
        aFontFace, std::move(attr), StyleOrigin::Author);
    if (!entry) {
      return;
    }
    aFontFace->SetUserFontEntry(entry);
  }
  AddUserFontEntry(family, aFontFace->GetUserFontEntry());
}

void FontFaceSetImpl::UpdateUserFontEntry(gfxUserFontEntry* aEntry,
                                          gfxUserFontAttributes&& aAttr) {
  MOZ_ASSERT(NS_IsMainThread());

  bool resetFamilyName = !aEntry->mFamilyName.IsEmpty() &&
                         aEntry->mFamilyName != aAttr.mFamilyName;
  // aFontFace already has a user font entry, so we update its attributes
  // rather than creating a new one.
  aEntry->UpdateAttributes(std::move(aAttr));
  // If the family name has changed, remove the entry from its current family
  // and clear the mFamilyName field so it can be reset when added to a new
  // family.
  if (resetFamilyName) {
    RefPtr<gfxUserFontFamily> family = LookupFamily(aEntry->mFamilyName);
    if (family) {
      family->RemoveFontEntry(aEntry);
    }
    aEntry->mFamilyName.Truncate(0);
  }
}

class FontFaceSetImpl::UpdateUserFontEntryRunnable final
    : public WorkerMainThreadRunnable {
 public:
  UpdateUserFontEntryRunnable(FontFaceSetImpl* aSet, gfxUserFontEntry* aEntry,
                              gfxUserFontAttributes& aAttr)
      : WorkerMainThreadRunnable(
            GetCurrentThreadWorkerPrivate(),
            "FontFaceSetImpl :: FindOrCreateUserFontEntryFromFontFace"_ns),
        mSet(aSet),
        mEntry(aEntry),
        mAttr(aAttr) {}

  bool MainThreadRun() override {
    mSet->UpdateUserFontEntry(mEntry, std::move(mAttr));
    return true;
  }

 private:
  FontFaceSetImpl* mSet;
  gfxUserFontEntry* mEntry;
  gfxUserFontAttributes& mAttr;
};

// TODO(emilio): Should this take an nsAtom* aFamilyName instead?
//
// All callers have one handy.
/* static */
already_AddRefed<gfxUserFontEntry>
FontFaceSetImpl::FindOrCreateUserFontEntryFromFontFace(
    FontFaceImpl* aFontFace, gfxUserFontAttributes&& aAttr,
    StyleOrigin aOrigin) {
  FontFaceSetImpl* set = aFontFace->GetPrimaryFontFaceSet();

  RefPtr<gfxUserFontEntry> existingEntry = aFontFace->GetUserFontEntry();
  if (existingEntry) {
    if (NS_IsMainThread()) {
      set->UpdateUserFontEntry(existingEntry, std::move(aAttr));
    } else {
      auto task =
          MakeRefPtr<UpdateUserFontEntryRunnable>(set, existingEntry, aAttr);
      IgnoredErrorResult ignoredRv;
      task->Dispatch(Canceling, ignoredRv);
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
    face->mBuffer = aFontFace->TakeBufferSource();
  } else {
    size_t len = aAttr.mSources.Length();
    for (size_t i = 0; i < len; ++i) {
      gfxFontFaceSrc* face = srcArray.AppendElement();
      const auto& component = aAttr.mSources[i];
      switch (component.tag) {
        case StyleFontFaceSourceListComponent::Tag::Local: {
          nsAtom* atom = component.AsLocal();
          face->mLocalName.Append(nsAtomCString(atom));
          face->mSourceType = gfxFontFaceSrc::eSourceType_Local;
          face->mURI = nullptr;
          face->mFormatHint = StyleFontFaceSourceFormatKeyword::None;
          break;
        }

        case StyleFontFaceSourceListComponent::Tag::Url: {
          face->mSourceType = gfxFontFaceSrc::eSourceType_URL;
          const StyleCssUrl* url = component.AsUrl();
          nsIURI* uri = url->GetURI();
          face->mURI = uri ? new gfxFontSrcURI(uri) : nullptr;
          const URLExtraData& extraData = url->ExtraData();
          face->mReferrerInfo = extraData.ReferrerInfo();

          // agent and user stylesheets are treated slightly differently,
          // the same-site origin check and access control headers are
          // enforced against the sheet principal rather than the document
          // principal to allow user stylesheets to include @font-face rules
          if (aOrigin == StyleOrigin::User ||
              aOrigin == StyleOrigin::UserAgent) {
            face->mUseOriginPrincipal = true;
            face->mOriginPrincipal = new gfxFontSrcPrincipal(
                extraData.Principal(), extraData.Principal());
          }

          face->mLocalName.Truncate();
          face->mFormatHint = StyleFontFaceSourceFormatKeyword::None;
          face->mTechFlags = StyleFontFaceSourceTechFlags::Empty();

          if (i + 1 < len) {
            // Check for a format hint.
            const auto& next = aAttr.mSources[i + 1];
            switch (next.tag) {
              case StyleFontFaceSourceListComponent::Tag::FormatHintKeyword:
                face->mFormatHint = next.format_hint_keyword._0;
                i++;
                break;
              case StyleFontFaceSourceListComponent::Tag::FormatHintString: {
                nsDependentCSubstring valueString(
                    reinterpret_cast<const char*>(
                        next.format_hint_string.utf8_bytes),
                    next.format_hint_string.length);

                if (valueString.LowerCaseEqualsASCII("woff")) {
                  face->mFormatHint = StyleFontFaceSourceFormatKeyword::Woff;
                } else if (valueString.LowerCaseEqualsASCII("woff2")) {
                  face->mFormatHint = StyleFontFaceSourceFormatKeyword::Woff2;
                } else if (valueString.LowerCaseEqualsASCII("opentype")) {
                  face->mFormatHint =
                      StyleFontFaceSourceFormatKeyword::Opentype;
                } else if (valueString.LowerCaseEqualsASCII("truetype")) {
                  face->mFormatHint =
                      StyleFontFaceSourceFormatKeyword::Truetype;
                } else if (valueString.LowerCaseEqualsASCII("truetype-aat")) {
                  face->mFormatHint =
                      StyleFontFaceSourceFormatKeyword::Truetype;
                } else if (valueString.LowerCaseEqualsASCII(
                               "embedded-opentype")) {
                  face->mFormatHint =
                      StyleFontFaceSourceFormatKeyword::EmbeddedOpentype;
                } else if (valueString.LowerCaseEqualsASCII("svg")) {
                  face->mFormatHint = StyleFontFaceSourceFormatKeyword::Svg;
                } else if (StaticPrefs::layout_css_font_variations_enabled()) {
                  // Non-standard values that Firefox accepted, for back-compat;
                  // these are superseded by the tech() function.
                  if (valueString.LowerCaseEqualsASCII("woff-variations")) {
                    face->mFormatHint = StyleFontFaceSourceFormatKeyword::Woff;
                  } else if (valueString.LowerCaseEqualsASCII(
                                 "woff2-variations")) {
                    face->mFormatHint = StyleFontFaceSourceFormatKeyword::Woff2;
                  } else if (valueString.LowerCaseEqualsASCII(
                                 "opentype-variations")) {
                    face->mFormatHint =
                        StyleFontFaceSourceFormatKeyword::Opentype;
                  } else if (valueString.LowerCaseEqualsASCII(
                                 "truetype-variations")) {
                    face->mFormatHint =
                        StyleFontFaceSourceFormatKeyword::Truetype;
                  } else {
                    face->mFormatHint =
                        StyleFontFaceSourceFormatKeyword::Unknown;
                  }
                } else {
                  // unknown format specified, mark to distinguish from the
                  // case where no format hints are specified
                  face->mFormatHint = StyleFontFaceSourceFormatKeyword::Unknown;
                }
                i++;
                break;
              }
              case StyleFontFaceSourceListComponent::Tag::TechFlags:
              case StyleFontFaceSourceListComponent::Tag::Local:
              case StyleFontFaceSourceListComponent::Tag::Url:
                break;
            }
          }

          if (i + 1 < len) {
            // Check for a set of font-technologies flags.
            const auto& next = aAttr.mSources[i + 1];
            if (next.IsTechFlags()) {
              face->mTechFlags = next.AsTechFlags();
              i++;
            }
          }

          if (!face->mURI) {
            // if URI not valid, omit from src array
            srcArray.RemoveLastElement();
            NS_WARNING("null url in @font-face rule");
            continue;
          }
          break;
        }

        case StyleFontFaceSourceListComponent::Tag::FormatHintKeyword:
        case StyleFontFaceSourceListComponent::Tag::FormatHintString:
        case StyleFontFaceSourceListComponent::Tag::TechFlags:
          MOZ_ASSERT_UNREACHABLE(
              "Should always come after a URL source, and be consumed already");
          break;
      }
    }
  }

  if (srcArray.IsEmpty()) {
    return nullptr;
  }

  return set->FindOrCreateUserFontEntry(std::move(srcArray), std::move(aAttr));
}

nsresult FontFaceSetImpl::LogMessage(gfxUserFontEntry* aUserFontEntry,
                                     uint32_t aSrcIndex, const char* aMessage,
                                     uint32_t aFlags, nsresult aStatus) {
  nsAutoCString familyName;
  nsAutoCString fontURI;
  aUserFontEntry->GetFamilyNameAndURIForLogging(aSrcIndex, familyName, fontURI);

  nsAutoCString weightString;
  aUserFontEntry->Weight().ToString(weightString);
  nsAutoCString stretchString;
  aUserFontEntry->Stretch().ToString(stretchString);
  nsPrintfCString message(
      "downloadable font: %s "
      "(font-family: \"%s\" style:%s weight:%s stretch:%s src index:%d)",
      aMessage, familyName.get(),
      aUserFontEntry->IsItalic() ? "italic" : "normal",  // XXX todo: oblique?
      weightString.get(), stretchString.get(), aSrcIndex);

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

  LOG(("userfonts (%p) %s", this, message.get()));

  if (GetCurrentThreadWorkerPrivate()) {
    // TODO(aosmond): Log to the console for workers. See bug 1778537.
    return NS_OK;
  }

  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // try to give the user an indication of where the rule came from
  StyleLockedFontFaceRule* rule = FindRuleForUserFontEntry(aUserFontEntry);
  nsString href;
  nsAutoCString text;
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
    // Leave href empty if we don't know how to get the correct sheet.
  }

  nsresult rv;
  nsCOMPtr<nsIScriptError> scriptError =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = scriptError->InitWithWindowID(NS_ConvertUTF8toUTF16(message),
                                     href,                         // file
                                     NS_ConvertUTF8toUTF16(text),  // src line
                                     line, column,
                                     aFlags,        // flags
                                     "CSS Loader",  // category (make separate?)
                                     GetInnerWindowID());
  if (NS_SUCCEEDED(rv)) {
    console->LogMessage(scriptError);
  }

  return NS_OK;
}

nsresult FontFaceSetImpl::SyncLoadFontData(gfxUserFontEntry* aFontToLoad,
                                           const gfxFontFaceSrc* aFontFaceSrc,
                                           uint8_t*& aBuffer,
                                           uint32_t& aBufferLength) {
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = CreateChannelForSyncLoadFontData(getter_AddRefs(channel),
                                                 aFontToLoad, aFontFaceSrc);
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

void FontFaceSetImpl::OnFontFaceStatusChanged(FontFaceImpl* aFontFace) {
  gfxFontUtils::AssertSafeThreadOrServoFontMetricsLocked();
  RecursiveMutexAutoLock lock(mMutex);
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

void FontFaceSetImpl::DispatchCheckLoadingFinishedAfterDelay() {
  gfxFontUtils::AssertSafeThreadOrServoFontMetricsLocked();

  if (ServoStyleSet* set = gfxFontUtils::CurrentServoStyleSet()) {
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

  DispatchToOwningThread(
      "FontFaceSetImpl::DispatchCheckLoadingFinishedAfterDelay",
      [self = RefPtr{this}]() { self->CheckLoadingFinishedAfterDelay(); });
}

void FontFaceSetImpl::CheckLoadingFinishedAfterDelay() {
  RecursiveMutexAutoLock lock(mMutex);
  mDelayedLoadCheck = false;
  CheckLoadingFinished();
}

void FontFaceSetImpl::CheckLoadingStarted() {
  gfxFontUtils::AssertSafeThreadOrServoFontMetricsLocked();
  RecursiveMutexAutoLock lock(mMutex);

  if (!HasLoadingFontFaces()) {
    return;
  }

  if (mStatus == FontFaceSetLoadStatus::Loading) {
    // We have already dispatched a loading event and replaced mReady
    // with a fresh, unresolved promise.
    return;
  }

  mStatus = FontFaceSetLoadStatus::Loading;

  if (IsOnOwningThread()) {
    OnLoadingStarted();
    return;
  }

  DispatchToOwningThread("FontFaceSetImpl::CheckLoadingStarted",
                         [self = RefPtr{this}]() { self->OnLoadingStarted(); });
}

void FontFaceSetImpl::OnLoadingStarted() {
  RecursiveMutexAutoLock lock(mMutex);
  if (mOwner) {
    mOwner->DispatchLoadingEventAndReplaceReadyPromise();
  }
}

void FontFaceSetImpl::UpdateHasLoadingFontFaces() {
  RecursiveMutexAutoLock lock(mMutex);
  mHasLoadingFontFacesIsDirty = false;
  mHasLoadingFontFaces = false;
  for (size_t i = 0; i < mNonRuleFaces.Length(); i++) {
    if (mNonRuleFaces[i].mFontFace->Status() == FontFaceLoadStatus::Loading) {
      mHasLoadingFontFaces = true;
      return;
    }
  }
}

bool FontFaceSetImpl::HasLoadingFontFaces() {
  RecursiveMutexAutoLock lock(mMutex);
  if (mHasLoadingFontFacesIsDirty) {
    UpdateHasLoadingFontFaces();
  }
  return mHasLoadingFontFaces;
}

bool FontFaceSetImpl::MightHavePendingFontLoads() {
  // Check for FontFace objects in the FontFaceSet that are still loading.
  return HasLoadingFontFaces();
}

void FontFaceSetImpl::CheckLoadingFinished() {
  RecursiveMutexAutoLock lock(mMutex);
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

  if (IsOnOwningThread()) {
    OnLoadingFinished();
    return;
  }

  DispatchToOwningThread(
      "FontFaceSetImpl::CheckLoadingFinished",
      [self = RefPtr{this}]() { self->OnLoadingFinished(); });
}

void FontFaceSetImpl::OnLoadingFinished() {
  RecursiveMutexAutoLock lock(mMutex);
  if (mOwner) {
    mOwner->MaybeResolve();
  }
}

void FontFaceSetImpl::RefreshStandardFontLoadPrincipal() {
  RecursiveMutexAutoLock lock(mMutex);
  mAllowedFontLoads.Clear();
  IncrementGeneration(false);
}

// -- gfxUserFontSet
// ------------------------------------------------

already_AddRefed<gfxFontSrcPrincipal>
FontFaceSetImpl::GetStandardFontLoadPrincipal() const {
  RecursiveMutexAutoLock lock(mMutex);
  return RefPtr{mStandardFontLoadPrincipal}.forget();
}

void FontFaceSetImpl::RecordFontLoadDone(uint32_t aFontSize,
                                         TimeStamp aDoneTime) {
  mDownloadCount++;
  mDownloadSize += aFontSize;
  Telemetry::Accumulate(Telemetry::WEBFONT_SIZE, aFontSize / 1024);

  TimeStamp navStart = GetNavigationStartTimeStamp();
  TimeStamp zero;
  if (navStart != zero) {
    Telemetry::AccumulateTimeDelta(Telemetry::WEBFONT_DOWNLOAD_TIME_AFTER_START,
                                   navStart, aDoneTime);
  }
}

void FontFaceSetImpl::DoRebuildUserFontSet() { MarkUserFontSetDirty(); }

already_AddRefed<gfxUserFontEntry> FontFaceSetImpl::CreateUserFontEntry(
    nsTArray<gfxFontFaceSrc>&& aFontFaceSrcList,
    gfxUserFontAttributes&& aAttr) {
  RefPtr<gfxUserFontEntry> entry = new FontFaceImpl::Entry(
      this, std::move(aFontFaceSrcList), std::move(aAttr));
  return entry.forget();
}

void FontFaceSetImpl::ForgetLocalFaces() {
  // We cannot hold our lock at the same time as the gfxUserFontFamily lock, so
  // we need to make a copy of the table first.
  nsTArray<RefPtr<gfxUserFontFamily>> fontFamilies;
  {
    RecursiveMutexAutoLock lock(mMutex);
    fontFamilies.SetCapacity(mFontFamilies.Count());
    for (const auto& fam : mFontFamilies.Values()) {
      fontFamilies.AppendElement(fam);
    }
  }

  for (const auto& fam : fontFamilies) {
    ForgetLocalFace(fam);
  }
}

already_AddRefed<gfxUserFontFamily> FontFaceSetImpl::GetFamily(
    const nsACString& aFamilyName) {
  RecursiveMutexAutoLock lock(mMutex);
  return gfxUserFontSet::GetFamily(aFamilyName);
}

#undef LOG_ENABLED
#undef LOG
