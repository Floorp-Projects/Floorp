/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginTags.h"

#include "prlink.h"
#include "plstr.h"
#include "prenv.h"
#include "nsPluginHost.h"
#include "nsIBlocklistService.h"
#include "nsPluginLogging.h"
#include "nsCharSeparatedTokenizer.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsNetUtil.h"
#include "prenv.h"
#include <cctype>
#include "mozilla/Encoding.h"
#include "mozilla/dom/FakePluginTagInitBinding.h"
#include "mozilla/StaticPrefs_plugin.h"

using mozilla::dom::FakePluginTagInit;
using namespace mozilla;

// check comma delimited extensions
static bool ExtensionInList(const nsCString& aExtensionList,
                            const nsACString& aExtension) {
  for (const nsACString& extension :
       nsCCharSeparatedTokenizer(aExtensionList, ',').ToRange()) {
    if (extension.Equals(aExtension, nsCaseInsensitiveCStringComparator)) {
      return true;
    }
  }
  return false;
}

// Search for an extension in an extensions array, and return its
// matching mime type
static bool SearchExtensions(const nsTArray<nsCString>& aExtensions,
                             const nsTArray<nsCString>& aMimeTypes,
                             const nsACString& aFindExtension,
                             nsACString& aMatchingType) {
  uint32_t mimes = aMimeTypes.Length();
  MOZ_ASSERT(mimes == aExtensions.Length(),
             "These arrays should have matching elements");

  aMatchingType.Truncate();

  for (uint32_t i = 0; i < mimes; i++) {
    if (ExtensionInList(aExtensions[i], aFindExtension)) {
      aMatchingType = aMimeTypes[i];
      return true;
    }
  }

  return false;
}

static nsCString MakeNiceFileName(const nsCString& aFileName) {
  nsCString niceName = aFileName;
  int32_t niceNameLength = aFileName.RFind(".");
  NS_ASSERTION(niceNameLength != kNotFound, "aFileName doesn't have a '.'?");
  while (niceNameLength > 0) {
    char chr = aFileName[niceNameLength - 1];
    if (!std::isalpha(chr))
      niceNameLength--;
    else
      break;
  }

  // If it turns out that niceNameLength <= 0, we'll fall back and use the
  // entire aFileName (which we've already taken care of, a few lines back).
  if (niceNameLength > 0) {
    niceName.Truncate(niceNameLength);
  }

  ToLowerCase(niceName);
  return niceName;
}

static nsCString MakePrefNameForPlugin(const char* const subname,
                                       nsIInternalPluginTag* aTag) {
  nsCString pref;
  nsAutoCString pluginName(aTag->GetNiceFileName());

  if (pluginName.IsEmpty()) {
    // Use filename if nice name fails
    pluginName = aTag->FileName();
    if (pluginName.IsEmpty()) {
      MOZ_ASSERT_UNREACHABLE("Plugin with no filename or nice name in list");
      pluginName.AssignLiteral("unknown-plugin-name");
    }
  }

  pref.AssignLiteral("plugin.");
  pref.Append(subname);
  pref.Append('.');
  pref.Append(pluginName);

  return pref;
}

static nsCString GetStatePrefNameForPlugin(nsIInternalPluginTag* aTag) {
  return MakePrefNameForPlugin("state", aTag);
}

static nsresult IsEnabledStateLockedForPlugin(nsIInternalPluginTag* aTag,
                                              bool* aIsEnabledStateLocked) {
  *aIsEnabledStateLocked = false;
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));

  if (NS_WARN_IF(!prefs)) {
    return NS_ERROR_FAILURE;
  }

  Unused << prefs->PrefIsLocked(GetStatePrefNameForPlugin(aTag).get(),
                                aIsEnabledStateLocked);

  return NS_OK;
}

/* nsIInternalPluginTag */

uint32_t nsIInternalPluginTag::sNextId;

nsIInternalPluginTag::nsIInternalPluginTag() = default;

nsIInternalPluginTag::nsIInternalPluginTag(const char* aName,
                                           const char* aDescription,
                                           const char* aFileName,
                                           const char* aVersion)
    : mName(aName),
      mDescription(aDescription),
      mFileName(aFileName),
      mVersion(aVersion) {}

nsIInternalPluginTag::nsIInternalPluginTag(
    const char* aName, const char* aDescription, const char* aFileName,
    const char* aVersion, const nsTArray<nsCString>& aMimeTypes,
    const nsTArray<nsCString>& aMimeDescriptions,
    const nsTArray<nsCString>& aExtensions)
    : mName(aName),
      mDescription(aDescription),
      mFileName(aFileName),
      mVersion(aVersion),
      mMimeTypes(aMimeTypes.Clone()),
      mMimeDescriptions(aMimeDescriptions.Clone()),
      mExtensions(aExtensions.Clone()) {}

nsIInternalPluginTag::~nsIInternalPluginTag() = default;

bool nsIInternalPluginTag::HasExtension(const nsACString& aExtension,
                                        nsACString& aMatchingType) const {
  return SearchExtensions(mExtensions, mMimeTypes, aExtension, aMatchingType);
}

bool nsIInternalPluginTag::HasMimeType(const nsACString& aMimeType) const {
  return mMimeTypes.Contains(aMimeType,
                             nsCaseInsensitiveCStringArrayComparator());
}

/* nsFakePluginTag */

nsFakePluginTag::nsFakePluginTag()
    : mId(sNextId++), mState(ePluginState_Disabled) {}

nsFakePluginTag::nsFakePluginTag(uint32_t aId,
                                 already_AddRefed<nsIURI>&& aHandlerURI,
                                 const char* aName, const char* aDescription,
                                 const nsTArray<nsCString>& aMimeTypes,
                                 const nsTArray<nsCString>& aMimeDescriptions,
                                 const nsTArray<nsCString>& aExtensions,
                                 const nsCString& aNiceName,
                                 const nsString& aSandboxScript)
    : nsIInternalPluginTag(aName, aDescription, nullptr, nullptr, aMimeTypes,
                           aMimeDescriptions, aExtensions),
      mId(aId),
      mHandlerURI(aHandlerURI),
      mNiceName(aNiceName),
      mSandboxScript(aSandboxScript),
      mState(ePluginState_Enabled) {}

nsFakePluginTag::~nsFakePluginTag() = default;

NS_IMPL_ADDREF(nsFakePluginTag)
NS_IMPL_RELEASE(nsFakePluginTag)
NS_INTERFACE_TABLE_HEAD(nsFakePluginTag)
  NS_INTERFACE_TABLE_BEGIN
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsFakePluginTag, nsIPluginTag,
                                       nsIInternalPluginTag)
    NS_INTERFACE_TABLE_ENTRY(nsFakePluginTag, nsIInternalPluginTag)
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsFakePluginTag, nsISupports,
                                       nsIInternalPluginTag)
    NS_INTERFACE_TABLE_ENTRY(nsFakePluginTag, nsIFakePluginTag)
  NS_INTERFACE_TABLE_END
NS_INTERFACE_TABLE_TAIL

/* static */
nsresult nsFakePluginTag::Create(const FakePluginTagInit& aInitDictionary,
                                 nsFakePluginTag** aPluginTag) {
  NS_ENSURE_TRUE(sNextId <= PR_INT32_MAX, NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(!aInitDictionary.mMimeEntries.IsEmpty(), NS_ERROR_INVALID_ARG);

  RefPtr<nsFakePluginTag> tag = new nsFakePluginTag();
  nsresult rv =
      NS_NewURI(getter_AddRefs(tag->mHandlerURI), aInitDictionary.mHandlerURI);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUTF16toUTF8(aInitDictionary.mNiceName, tag->mNiceName);
  CopyUTF16toUTF8(aInitDictionary.mFullPath, tag->mFullPath);
  CopyUTF16toUTF8(aInitDictionary.mName, tag->mName);
  CopyUTF16toUTF8(aInitDictionary.mDescription, tag->mDescription);
  CopyUTF16toUTF8(aInitDictionary.mFileName, tag->mFileName);
  CopyUTF16toUTF8(aInitDictionary.mVersion, tag->mVersion);
  tag->mSandboxScript = aInitDictionary.mSandboxScript;

  for (const mozilla::dom::FakePluginMimeEntry& mimeEntry :
       aInitDictionary.mMimeEntries) {
    CopyUTF16toUTF8(mimeEntry.mType, *tag->mMimeTypes.AppendElement());
    CopyUTF16toUTF8(mimeEntry.mDescription,
                    *tag->mMimeDescriptions.AppendElement());
    CopyUTF16toUTF8(mimeEntry.mExtension, *tag->mExtensions.AppendElement());
  }

  tag.forget(aPluginTag);
  return NS_OK;
}

bool nsFakePluginTag::HandlerURIMatches(nsIURI* aURI) {
  bool equals = false;
  return NS_SUCCEEDED(mHandlerURI->Equals(aURI, &equals)) && equals;
}

NS_IMETHODIMP
nsFakePluginTag::GetHandlerURI(nsIURI** aResult) {
  NS_IF_ADDREF(*aResult = mHandlerURI);
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetSandboxScript(nsAString& aSandboxScript) {
  aSandboxScript = mSandboxScript;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetDescription(/* utf-8 */ nsACString& aResult) {
  aResult = mDescription;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetIsFlashPlugin(bool* aIsFlash) {
  *aIsFlash = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetFilename(/* utf-8 */ nsACString& aResult) {
  aResult = mFileName;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetFullpath(/* utf-8 */ nsACString& aResult) {
  aResult = mFullPath;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetVersion(/* utf-8 */ nsACString& aResult) {
  aResult = mVersion;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetName(/* utf-8 */ nsACString& aResult) {
  aResult = mName;
  return NS_OK;
}

const nsCString& nsFakePluginTag::GetNiceFileName() {
  // We don't try to mimic the special-cased flash/java names if the fake plugin
  // claims one of their MIME types, but do allow directly setting niceName if
  // emulating those is desired.
  if (mNiceName.IsEmpty() && !mFileName.IsEmpty()) {
    mNiceName = MakeNiceFileName(mFileName);
  }

  return mNiceName;
}

NS_IMETHODIMP
nsFakePluginTag::GetNiceName(/* utf-8 */ nsACString& aResult) {
  aResult = GetNiceFileName();
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetBlocklistState(uint32_t* aResult) {
  // Fake tags don't currently support blocklisting
  *aResult = nsIBlocklistService::STATE_NOT_BLOCKED;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetBlocklisted(bool* aBlocklisted) {
  // Fake tags can't be blocklisted
  *aBlocklisted = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetIsEnabledStateLocked(bool* aIsEnabledStateLocked) {
  return IsEnabledStateLockedForPlugin(this, aIsEnabledStateLocked);
}

bool nsFakePluginTag::IsEnabled() {
  return mState == ePluginState_Enabled || mState == ePluginState_Clicktoplay;
}

NS_IMETHODIMP
nsFakePluginTag::GetDisabled(bool* aDisabled) {
  *aDisabled = !IsEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetClicktoplay(bool* aClicktoplay) {
  *aClicktoplay = (mState == ePluginState_Clicktoplay);
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetEnabledState(uint32_t* aEnabledState) {
  *aEnabledState = (uint32_t)mState;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::SetEnabledState(uint32_t aEnabledState) {
  // There are static asserts above enforcing that this enum matches
  mState = (PluginState)aEnabledState;
  // FIXME-jsplugins update
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetMimeTypes(nsTArray<nsCString>& aResults) {
  aResults = mMimeTypes.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetMimeDescriptions(nsTArray<nsCString>& aResults) {
  aResults = mMimeDescriptions.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetExtensions(nsTArray<nsCString>& aResults) {
  aResults = mExtensions.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetActive(bool* aResult) {
  // Fake plugins can't be blocklisted, so this is just !Disabled
  *aResult = IsEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetLastModifiedTime(PRTime* aLastModifiedTime) {
  // FIXME-jsplugins What should this return, if anything?
  MOZ_ASSERT(aLastModifiedTime);
  *aLastModifiedTime = 0;
  return NS_OK;
}

// We don't load fake plugins out of a library, so they should always be there.
NS_IMETHODIMP
nsFakePluginTag::GetLoaded(bool* ret) {
  *ret = true;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetId(uint32_t* aId) {
  *aId = mId;
  return NS_OK;
}
