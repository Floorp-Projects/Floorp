/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginTags.h"

#include "prlink.h"
#include "plstr.h"
#include "nsIPluginInstanceOwner.h"
#include "nsPluginsDir.h"
#include "nsPluginHost.h"
#include "nsIBlocklistService.h"
#include "nsIUnicodeDecoder.h"
#include "nsIPlatformCharset.h"
#include "nsPluginLogging.h"
#include "nsNPAPIPlugin.h"
#include "nsCharSeparatedTokenizer.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"
#include <cctype>
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/ContentChild.h"

using mozilla::dom::EncodingUtils;
using namespace mozilla;

// These legacy flags are used in the plugin registry. The states are now
// stored in prefs, but we still need to be able to import them.
#define NS_PLUGIN_FLAG_ENABLED      0x0001    // is this plugin enabled?
// no longer used                   0x0002    // reuse only if regenerating pluginreg.dat
#define NS_PLUGIN_FLAG_FROMCACHE    0x0004    // this plugintag info was loaded from cache
// no longer used                   0x0008    // reuse only if regenerating pluginreg.dat
#define NS_PLUGIN_FLAG_CLICKTOPLAY  0x0020    // this is a click-to-play plugin

static const char kPrefDefaultEnabledState[] = "plugin.default.state";
static const char kPrefDefaultEnabledStateXpi[] = "plugin.defaultXpi.state";

// check comma delimited extensions
static bool ExtensionInList(const nsCString& aExtensionList,
                            const nsACString& aExtension)
{
  nsCCharSeparatedTokenizer extensions(aExtensionList, ',');
  while (extensions.hasMoreTokens()) {
    const nsCSubstring& extension = extensions.nextToken();
    if (extension.Equals(aExtension, nsCaseInsensitiveCStringComparator())) {
      return true;
    }
  }
  return false;
}

// Search for an extension in an extensions array, and return its
// matching mime type
static bool SearchExtensions(const nsTArray<nsCString> & aExtensions,
                             const nsTArray<nsCString> & aMimeTypes,
                             const nsACString & aFindExtension,
                             nsACString & aMatchingType)
{
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

static nsCString
MakeNiceFileName(const nsCString & aFileName)
{
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

static nsCString
MakePrefNameForPlugin(const char* const subname, nsPluginTag* aTag)
{
  nsCString pref;

  pref.AssignLiteral("plugin.");
  pref.Append(subname);
  pref.Append('.');
  pref.Append(aTag->GetNiceFileName());

  return pref;
}

static nsresult
CStringArrayToXPCArray(nsTArray<nsCString> & aArray,
                       uint32_t* aCount,
                       char16_t*** aResults)
{
  uint32_t count = aArray.Length();
  if (!count) {
    *aResults = nullptr;
    *aCount = 0;
    return NS_OK;
  }

  *aResults =
    static_cast<char16_t**>(moz_xmalloc(count * sizeof(**aResults)));
  *aCount = count;

  for (uint32_t i = 0; i < count; i++) {
    (*aResults)[i] = ToNewUnicode(NS_ConvertUTF8toUTF16(aArray[i]));
  }

  return NS_OK;
}

static nsCString
GetStatePrefNameForPlugin(nsPluginTag* aTag)
{
  return MakePrefNameForPlugin("state", aTag);
}

/* nsPluginTag */

uint32_t nsPluginTag::sNextId;

nsPluginTag::nsPluginTag(nsPluginInfo* aPluginInfo,
                         int64_t aLastModifiedTime,
                         bool fromExtension)
  : mId(sNextId++),
    mContentProcessRunningCount(0),
    mHadLocalInstance(false),
    mName(aPluginInfo->fName),
    mDescription(aPluginInfo->fDescription),
    mLibrary(nullptr),
    mIsJavaPlugin(false),
    mIsFlashPlugin(false),
    mFileName(aPluginInfo->fFileName),
    mFullPath(aPluginInfo->fFullPath),
    mVersion(aPluginInfo->fVersion),
    mLastModifiedTime(aLastModifiedTime),
    mCachedBlocklistState(nsIBlocklistService::STATE_NOT_BLOCKED),
    mCachedBlocklistStateValid(false),
    mIsFromExtension(fromExtension)
{
  InitMime(aPluginInfo->fMimeTypeArray,
           aPluginInfo->fMimeDescriptionArray,
           aPluginInfo->fExtensionArray,
           aPluginInfo->fVariantCount);
  EnsureMembersAreUTF8();
  FixupVersion();
}

nsPluginTag::nsPluginTag(const char* aName,
                         const char* aDescription,
                         const char* aFileName,
                         const char* aFullPath,
                         const char* aVersion,
                         const char* const* aMimeTypes,
                         const char* const* aMimeDescriptions,
                         const char* const* aExtensions,
                         int32_t aVariants,
                         int64_t aLastModifiedTime,
                         bool fromExtension,
                         bool aArgsAreUTF8)
  : mId(sNextId++),
    mContentProcessRunningCount(0),
    mName(aName),
    mDescription(aDescription),
    mLibrary(nullptr),
    mIsJavaPlugin(false),
    mIsFlashPlugin(false),
    mFileName(aFileName),
    mFullPath(aFullPath),
    mVersion(aVersion),
    mLastModifiedTime(aLastModifiedTime),
    mCachedBlocklistState(nsIBlocklistService::STATE_NOT_BLOCKED),
    mCachedBlocklistStateValid(false),
    mIsFromExtension(fromExtension)
{
  InitMime(aMimeTypes, aMimeDescriptions, aExtensions,
           static_cast<uint32_t>(aVariants));
  if (!aArgsAreUTF8)
    EnsureMembersAreUTF8();
  FixupVersion();
}

nsPluginTag::nsPluginTag(uint32_t aId,
                         const char* aName,
                         const char* aDescription,
                         const char* aFileName,
                         const char* aFullPath,
                         const char* aVersion,
                         nsTArray<nsCString> aMimeTypes,
                         nsTArray<nsCString> aMimeDescriptions,
                         nsTArray<nsCString> aExtensions,
                         bool aIsJavaPlugin,
                         bool aIsFlashPlugin,
                         int64_t aLastModifiedTime,
                         bool aFromExtension)
  : mId(aId),
    mContentProcessRunningCount(0),
    mName(aName),
    mDescription(aDescription),
    mMimeTypes(aMimeTypes),
    mMimeDescriptions(aMimeDescriptions),
    mExtensions(aExtensions),
    mLibrary(nullptr),
    mIsJavaPlugin(aIsJavaPlugin),
    mIsFlashPlugin(aIsFlashPlugin),
    mFileName(aFileName),
    mVersion(aVersion),
    mLastModifiedTime(aLastModifiedTime),
    mNiceFileName(),
    mCachedBlocklistState(nsIBlocklistService::STATE_NOT_BLOCKED),
    mCachedBlocklistStateValid(false),
    mIsFromExtension(aFromExtension)
{
}

nsPluginTag::~nsPluginTag()
{
  NS_ASSERTION(!mNext, "Risk of exhausting the stack space, bug 486349");
}

NS_IMPL_ISUPPORTS(nsPluginTag, nsIPluginTag)

void nsPluginTag::InitMime(const char* const* aMimeTypes,
                           const char* const* aMimeDescriptions,
                           const char* const* aExtensions,
                           uint32_t aVariantCount)
{
  if (!aMimeTypes) {
    return;
  }

  for (uint32_t i = 0; i < aVariantCount; i++) {
    if (!aMimeTypes[i]) {
      continue;
    }

    nsAutoCString mimeType(aMimeTypes[i]);

    // Convert the MIME type, which is case insensitive, to lowercase in order
    // to properly handle a mixed-case type.
    ToLowerCase(mimeType);

    if (!nsPluginHost::IsTypeWhitelisted(mimeType.get())) {
      continue;
    }

    // Look for certain special plugins.
    switch (nsPluginHost::GetSpecialType(mimeType)) {
      case nsPluginHost::eSpecialType_Java:
        mIsJavaPlugin = true;
        break;
      case nsPluginHost::eSpecialType_Flash:
        mIsFlashPlugin = true;
        break;
      case nsPluginHost::eSpecialType_None:
      default:
        break;
    }

    // Fill in our MIME type array.
    mMimeTypes.AppendElement(mimeType);

    // Now fill in the MIME descriptions.
    if (aMimeDescriptions && aMimeDescriptions[i]) {
      // we should cut off the list of suffixes which the mime
      // description string may have, see bug 53895
      // it is usually in form "some description (*.sf1, *.sf2)"
      // so we can search for the opening round bracket
      char cur = '\0';
      char pre = '\0';
      char * p = PL_strrchr(aMimeDescriptions[i], '(');
      if (p && (p != aMimeDescriptions[i])) {
        if ((p - 1) && *(p - 1) == ' ') {
          pre = *(p - 1);
          *(p - 1) = '\0';
        } else {
          cur = *p;
          *p = '\0';
        }
      }
      mMimeDescriptions.AppendElement(nsCString(aMimeDescriptions[i]));
      // restore the original string
      if (cur != '\0') {
        *p = cur;
      }
      if (pre != '\0') {
        *(p - 1) = pre;
      }
    } else {
      mMimeDescriptions.AppendElement(nsCString());
    }

    // Now fill in the extensions.
    if (aExtensions && aExtensions[i]) {
      mExtensions.AppendElement(nsCString(aExtensions[i]));
    } else {
      mExtensions.AppendElement(nsCString());
    }
  }
}

#if !defined(XP_WIN) && !defined(XP_MACOSX)
static nsresult ConvertToUTF8(nsIUnicodeDecoder *aUnicodeDecoder,
                              nsAFlatCString& aString)
{
  int32_t numberOfBytes = aString.Length();
  int32_t outUnicodeLen;
  nsAutoString buffer;
  nsresult rv = aUnicodeDecoder->GetMaxLength(aString.get(), numberOfBytes,
                                              &outUnicodeLen);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!buffer.SetLength(outUnicodeLen, fallible))
    return NS_ERROR_OUT_OF_MEMORY;
  rv = aUnicodeDecoder->Convert(aString.get(), &numberOfBytes,
                                buffer.BeginWriting(), &outUnicodeLen);
  NS_ENSURE_SUCCESS(rv, rv);
  buffer.SetLength(outUnicodeLen);
  CopyUTF16toUTF8(buffer, aString);

  return NS_OK;
}
#endif

nsresult nsPluginTag::EnsureMembersAreUTF8()
{
#if defined(XP_WIN) || defined(XP_MACOSX)
  return NS_OK;
#else
  nsresult rv;

  nsCOMPtr<nsIPlatformCharset> pcs =
  do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIUnicodeDecoder> decoder;

  nsAutoCString charset;
  rv = pcs->GetCharset(kPlatformCharsetSel_FileName, charset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!charset.LowerCaseEqualsLiteral("utf-8")) {
    decoder = EncodingUtils::DecoderForEncoding(charset);
    ConvertToUTF8(decoder, mFileName);
    ConvertToUTF8(decoder, mFullPath);
  }
  
  // The description of the plug-in and the various MIME type descriptions
  // should be encoded in the standard plain text file encoding for this system.
  // XXX should we add kPlatformCharsetSel_PluginResource?
  rv = pcs->GetCharset(kPlatformCharsetSel_PlainTextInFile, charset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!charset.LowerCaseEqualsLiteral("utf-8")) {
    decoder = EncodingUtils::DecoderForEncoding(charset);
    ConvertToUTF8(decoder, mName);
    ConvertToUTF8(decoder, mDescription);
    for (uint32_t i = 0; i < mMimeDescriptions.Length(); ++i) {
      ConvertToUTF8(decoder, mMimeDescriptions[i]);
    }
  }
  return NS_OK;
#endif
}

void nsPluginTag::FixupVersion()
{
#if defined(XP_LINUX)
  if (mIsFlashPlugin) {
    mVersion.ReplaceChar(',', '.');
  }
#endif
}

NS_IMETHODIMP
nsPluginTag::GetDescription(nsACString& aDescription)
{
  aDescription = mDescription;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetFilename(nsACString& aFileName)
{
  aFileName = mFileName;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetFullpath(nsACString& aFullPath)
{
  aFullPath = mFullPath;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetVersion(nsACString& aVersion)
{
  aVersion = mVersion;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetName(nsACString& aName)
{
  aName = mName;
  return NS_OK;
}

bool
nsPluginTag::IsActive()
{
  return IsEnabled() && !IsBlocklisted();
}

NS_IMETHODIMP
nsPluginTag::GetActive(bool *aResult)
{
  *aResult = IsActive();
  return NS_OK;
}

bool
nsPluginTag::IsEnabled()
{
  const PluginState state = GetPluginState();
  return (state == ePluginState_Enabled) || (state == ePluginState_Clicktoplay);
}

NS_IMETHODIMP
nsPluginTag::GetDisabled(bool* aDisabled)
{
  *aDisabled = !IsEnabled();
  return NS_OK;
}

bool
nsPluginTag::IsBlocklisted()
{
  uint32_t blocklistState;
  nsresult rv = GetBlocklistState(&blocklistState);
  return NS_FAILED(rv) || blocklistState == nsIBlocklistService::STATE_BLOCKED;
}

NS_IMETHODIMP
nsPluginTag::GetBlocklisted(bool* aBlocklisted)
{
  *aBlocklisted = IsBlocklisted();
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetIsEnabledStateLocked(bool* aIsEnabledStateLocked)
{
  *aIsEnabledStateLocked = false;
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));

  if (NS_WARN_IF(!prefs)) {
    return NS_ERROR_FAILURE;
  }

  unused << prefs->PrefIsLocked(GetStatePrefNameForPlugin(this).get(),
                                aIsEnabledStateLocked);

  return NS_OK;
}

bool
nsPluginTag::IsClicktoplay()
{
  const PluginState state = GetPluginState();
  return (state == ePluginState_Clicktoplay);
}

NS_IMETHODIMP
nsPluginTag::GetClicktoplay(bool *aClicktoplay)
{
  *aClicktoplay = IsClicktoplay();
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetEnabledState(uint32_t *aEnabledState) {
  int32_t enabledState;
  nsresult rv = Preferences::GetInt(GetStatePrefNameForPlugin(this).get(),
                                    &enabledState);
  if (NS_SUCCEEDED(rv) &&
      enabledState >= nsIPluginTag::STATE_DISABLED &&
      enabledState <= nsIPluginTag::STATE_ENABLED) {
    *aEnabledState = (uint32_t)enabledState;
    return rv;
  }

  const char* const pref = mIsFromExtension ? kPrefDefaultEnabledStateXpi
                                            : kPrefDefaultEnabledState;

  enabledState = Preferences::GetInt(pref, nsIPluginTag::STATE_ENABLED);
  if (enabledState >= nsIPluginTag::STATE_DISABLED &&
      enabledState <= nsIPluginTag::STATE_ENABLED) {
    *aEnabledState = (uint32_t)enabledState;
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsPluginTag::SetEnabledState(uint32_t aEnabledState) {
  if (aEnabledState >= ePluginState_MaxValue)
    return NS_ERROR_ILLEGAL_VALUE;
  uint32_t oldState = nsIPluginTag::STATE_DISABLED;
  GetEnabledState(&oldState);
  if (oldState != aEnabledState) {
    Preferences::SetInt(GetStatePrefNameForPlugin(this).get(), aEnabledState);
    if (nsRefPtr<nsPluginHost> host = nsPluginHost::GetInst()) {
      host->UpdatePluginInfo(this);
    }
  }
  return NS_OK;
}

nsPluginTag::PluginState
nsPluginTag::GetPluginState()
{
  uint32_t enabledState = nsIPluginTag::STATE_DISABLED;
  GetEnabledState(&enabledState);
  return (PluginState)enabledState;
}

void
nsPluginTag::SetPluginState(PluginState state)
{
  static_assert((uint32_t)nsPluginTag::ePluginState_Disabled == nsIPluginTag::STATE_DISABLED, "nsPluginTag::ePluginState_Disabled must match nsIPluginTag::STATE_DISABLED");
  static_assert((uint32_t)nsPluginTag::ePluginState_Clicktoplay == nsIPluginTag::STATE_CLICKTOPLAY, "nsPluginTag::ePluginState_Clicktoplay must match nsIPluginTag::STATE_CLICKTOPLAY");
  static_assert((uint32_t)nsPluginTag::ePluginState_Enabled == nsIPluginTag::STATE_ENABLED, "nsPluginTag::ePluginState_Enabled must match nsIPluginTag::STATE_ENABLED");
  SetEnabledState((uint32_t)state);
}

NS_IMETHODIMP
nsPluginTag::GetMimeTypes(uint32_t* aCount, char16_t*** aResults)
{
  return CStringArrayToXPCArray(mMimeTypes, aCount, aResults);
}

NS_IMETHODIMP
nsPluginTag::GetMimeDescriptions(uint32_t* aCount, char16_t*** aResults)
{
  return CStringArrayToXPCArray(mMimeDescriptions, aCount, aResults);
}

NS_IMETHODIMP
nsPluginTag::GetExtensions(uint32_t* aCount, char16_t*** aResults)
{
  return CStringArrayToXPCArray(mExtensions, aCount, aResults);
}

bool
nsPluginTag::HasSameNameAndMimes(const nsPluginTag *aPluginTag) const
{
  NS_ENSURE_TRUE(aPluginTag, false);

  if ((!mName.Equals(aPluginTag->mName)) ||
      (mMimeTypes.Length() != aPluginTag->mMimeTypes.Length())) {
    return false;
  }

  for (uint32_t i = 0; i < mMimeTypes.Length(); i++) {
    if (!mMimeTypes[i].Equals(aPluginTag->mMimeTypes[i])) {
      return false;
    }
  }

  return true;
}

void nsPluginTag::TryUnloadPlugin(bool inShutdown)
{
  // We never want to send NPP_Shutdown to an in-process plugin unless
  // this process is shutting down.
  if (!mPlugin) {
    return;
  }
  if (inShutdown || mPlugin->GetLibrary()->IsOOP()) {
    mPlugin->Shutdown();
    mPlugin = nullptr;
  }
}

nsCString nsPluginTag::GetNiceFileName() {
  if (!mNiceFileName.IsEmpty()) {
    return mNiceFileName;
  }

  if (mIsFlashPlugin) {
    mNiceFileName.AssignLiteral("flash");
    return mNiceFileName;
  }

  if (mIsJavaPlugin) {
    mNiceFileName.AssignLiteral("java");
    return mNiceFileName;
  }

  mNiceFileName = MakeNiceFileName(mFileName);
  return mNiceFileName;
}

NS_IMETHODIMP
nsPluginTag::GetNiceName(nsACString & aResult)
{
  aResult = GetNiceFileName();
  return NS_OK;
}

void nsPluginTag::ImportFlagsToPrefs(uint32_t flags)
{
  if (!(flags & NS_PLUGIN_FLAG_ENABLED)) {
    SetPluginState(ePluginState_Disabled);
  }
}

NS_IMETHODIMP
nsPluginTag::GetBlocklistState(uint32_t *aResult)
{
#if defined(MOZ_WIDGET_ANDROID)
  *aResult = nsIBlocklistService::STATE_NOT_BLOCKED;
  return NS_OK;
#else
  if (mCachedBlocklistStateValid) {
    *aResult = mCachedBlocklistState;
    return NS_OK;
  }

  if (!XRE_IsParentProcess()) {
    *aResult = nsIBlocklistService::STATE_BLOCKED;
    dom::ContentChild* cp = dom::ContentChild::GetSingleton();
    if (!cp->SendGetBlocklistState(mId, aResult)) {
      return NS_OK;
    }
  } else {
    nsCOMPtr<nsIBlocklistService> blocklist =
      do_GetService("@mozilla.org/extensions/blocklist;1");

    if (!blocklist) {
      *aResult = nsIBlocklistService::STATE_NOT_BLOCKED;
      return NS_OK;
    }

    // The EmptyString()s are so we use the currently running application
    // and toolkit versions
    if (NS_FAILED(blocklist->GetPluginBlocklistState(this, EmptyString(),
                                                     EmptyString(), aResult))) {
      *aResult = nsIBlocklistService::STATE_NOT_BLOCKED;
      return NS_OK;
    }
  }

  MOZ_ASSERT(*aResult <= UINT16_MAX);
  mCachedBlocklistState = (uint16_t) *aResult;
  mCachedBlocklistStateValid = true;
  return NS_OK;
#endif // defined(MOZ_WIDGET_ANDROID)
}

bool
nsPluginTag::HasMimeType(const nsACString & aMimeType) const
{
  return mMimeTypes.Contains(aMimeType,
                             nsCaseInsensitiveCStringArrayComparator());
}

bool
nsPluginTag::HasExtension(const nsACString & aExtension,
                          nsACString & aMatchingType) const
{
  return SearchExtensions(mExtensions, mMimeTypes, aExtension, aMatchingType);
}

void
nsPluginTag::InvalidateBlocklistState()
{
  mCachedBlocklistStateValid = false;
}

NS_IMETHODIMP
nsPluginTag::GetLastModifiedTime(PRTime* aLastModifiedTime)
{
  MOZ_ASSERT(aLastModifiedTime);
  *aLastModifiedTime = mLastModifiedTime;
  return NS_OK;
}

bool nsPluginTag::IsFromExtension() const
{
  return mIsFromExtension;
}
