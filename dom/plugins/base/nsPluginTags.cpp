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
#include "nsPluginLogging.h"
#include "nsNPAPIPlugin.h"
#include "nsCharSeparatedTokenizer.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsNetUtil.h"
#include <cctype>
#include "mozilla/Encoding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/FakePluginTagInitBinding.h"

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#include "mozilla/SandboxSettings.h"
#endif

using mozilla::dom::FakePluginTagInit;
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
    const nsACString& extension = extensions.nextToken();
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
MakePrefNameForPlugin(const char* const subname, nsIInternalPluginTag* aTag)
{
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
GetStatePrefNameForPlugin(nsIInternalPluginTag* aTag)
{
  return MakePrefNameForPlugin("state", aTag);
}

static nsresult
IsEnabledStateLockedForPlugin(nsIInternalPluginTag* aTag,
                              bool* aIsEnabledStateLocked)
{
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
nsIInternalPluginTag::nsIInternalPluginTag()
{
}

nsIInternalPluginTag::nsIInternalPluginTag(const char* aName,
                                           const char* aDescription,
                                           const char* aFileName,
                                           const char* aVersion)
  : mName(aName)
  , mDescription(aDescription)
  , mFileName(aFileName)
  , mVersion(aVersion)
{
}

nsIInternalPluginTag::nsIInternalPluginTag(const char* aName,
                                           const char* aDescription,
                                           const char* aFileName,
                                           const char* aVersion,
                                           const nsTArray<nsCString>& aMimeTypes,
                                           const nsTArray<nsCString>& aMimeDescriptions,
                                           const nsTArray<nsCString>& aExtensions)
  : mName(aName)
  , mDescription(aDescription)
  , mFileName(aFileName)
  , mVersion(aVersion)
  , mMimeTypes(aMimeTypes)
  , mMimeDescriptions(aMimeDescriptions)
  , mExtensions(aExtensions)
{
}

nsIInternalPluginTag::~nsIInternalPluginTag()
{
}

bool
nsIInternalPluginTag::HasExtension(const nsACString& aExtension,
                                   nsACString& aMatchingType) const
{
  return SearchExtensions(mExtensions, mMimeTypes, aExtension, aMatchingType);
}

bool
nsIInternalPluginTag::HasMimeType(const nsACString& aMimeType) const
{
  return mMimeTypes.Contains(aMimeType,
                             nsCaseInsensitiveCStringArrayComparator());
}

/* nsPluginTag */

uint32_t nsPluginTag::sNextId;

nsPluginTag::nsPluginTag(nsPluginInfo* aPluginInfo,
                         int64_t aLastModifiedTime,
                         bool fromExtension,
                         uint32_t aBlocklistState)
  : nsIInternalPluginTag(aPluginInfo->fName, aPluginInfo->fDescription,
                         aPluginInfo->fFileName, aPluginInfo->fVersion),
    mId(sNextId++),
    mContentProcessRunningCount(0),
    mHadLocalInstance(false),
    mLibrary(nullptr),
    mIsFlashPlugin(false),
    mSupportsAsyncRender(false),
    mFullPath(aPluginInfo->fFullPath),
    mLastModifiedTime(aLastModifiedTime),
    mSandboxLevel(0),
    mIsSandboxLoggingEnabled(false),
    mIsFromExtension(fromExtension),
    mBlocklistState(aBlocklistState)
{
  InitMime(aPluginInfo->fMimeTypeArray,
           aPluginInfo->fMimeDescriptionArray,
           aPluginInfo->fExtensionArray,
           aPluginInfo->fVariantCount);
  InitSandboxLevel();
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
                         uint32_t aBlocklistState,
                         bool aArgsAreUTF8)
  : nsIInternalPluginTag(aName, aDescription, aFileName, aVersion),
    mId(sNextId++),
    mContentProcessRunningCount(0),
    mHadLocalInstance(false),
    mLibrary(nullptr),
    mIsFlashPlugin(false),
    mSupportsAsyncRender(false),
    mFullPath(aFullPath),
    mLastModifiedTime(aLastModifiedTime),
    mSandboxLevel(0),
    mIsSandboxLoggingEnabled(false),
    mIsFromExtension(fromExtension),
    mBlocklistState(aBlocklistState)
{
  InitMime(aMimeTypes, aMimeDescriptions, aExtensions,
           static_cast<uint32_t>(aVariants));
  InitSandboxLevel();
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
                         bool aIsFlashPlugin,
                         bool aSupportsAsyncRender,
                         int64_t aLastModifiedTime,
                         bool aFromExtension,
                         int32_t aSandboxLevel,
                         uint32_t aBlocklistState)
  : nsIInternalPluginTag(aName,
                         aDescription,
                         aFileName,
                         aVersion,
                         aMimeTypes,
                         aMimeDescriptions,
                         aExtensions)
  , mId(aId)
  , mContentProcessRunningCount(0)
  , mHadLocalInstance(false)
  , mLibrary(nullptr)
  , mIsFlashPlugin(aIsFlashPlugin)
  , mSupportsAsyncRender(aSupportsAsyncRender)
  , mLastModifiedTime(aLastModifiedTime)
  , mSandboxLevel(aSandboxLevel)
  , mIsSandboxLoggingEnabled(false)
  , mNiceFileName()
  , mIsFromExtension(aFromExtension)
  , mBlocklistState(aBlocklistState)
{
}

nsPluginTag::~nsPluginTag()
{
  NS_ASSERTION(!mNext, "Risk of exhausting the stack space, bug 486349");
}

NS_IMPL_ISUPPORTS(nsPluginTag, nsPluginTag,  nsIInternalPluginTag, nsIPluginTag)

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
      case nsPluginHost::eSpecialType_Flash:
        // VLC sometimes claims to implement the Flash MIME type, and we want
        // to allow users to control that separately from Adobe Flash.
        if (Name().EqualsLiteral("Shockwave Flash")) {
          mIsFlashPlugin = true;
        }
        break;
      case nsPluginHost::eSpecialType_Test:
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

void
nsPluginTag::InitSandboxLevel()
{
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  nsAutoCString sandboxPref("dom.ipc.plugins.sandbox-level.");
  sandboxPref.Append(GetNiceFileName());
  if (NS_FAILED(Preferences::GetInt(sandboxPref.get(), &mSandboxLevel))) {
    mSandboxLevel = Preferences::GetInt("dom.ipc.plugins.sandbox-level.default"
);
  }

#if defined(_AMD64_)
  // Level 3 is now the default NPAPI sandbox level for 64-bit flash.
  // We permit the user to drop the sandbox level by at most 1.  This should
  // be kept up to date with the default value in the firefox.js pref file.
  if (mIsFlashPlugin && mSandboxLevel < 2) {
    mSandboxLevel = 2;
  }
#endif /* defined(_AMD64_) */

#elif defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  if (mIsFlashPlugin) {
    if (PR_GetEnv("MOZ_DISABLE_NPAPI_SANDBOX") ||
        NS_FAILED(Preferences::GetInt("dom.ipc.plugins.sandbox-level.flash",
                                      &mSandboxLevel))) {
      mSandboxLevel = 0;
    } else {
      mSandboxLevel = ClampFlashSandboxLevel(mSandboxLevel);
    }

    if (mSandboxLevel > 0) {
      // Enable sandbox logging in the plugin process if it has
      // been turned on via prefs or environment variables.
      if (Preferences::GetBool("security.sandbox.logging.enabled") ||
          PR_GetEnv("MOZ_SANDBOX_LOGGING") ||
          PR_GetEnv("MOZ_SANDBOX_MAC_FLASH_LOGGING")) {
            mIsSandboxLoggingEnabled = true;
      }
    }
  } else {
    // This isn't the flash plugin. At present, Flash is the only
    // supported plugin on macOS. Other test plugins are used during
    // testing and they will use the default plugin sandbox level.
    mSandboxLevel =
      Preferences::GetInt("dom.ipc.plugins.sandbox-level.default");
  }
#endif /* defined(XP_MACOSX) && defined(MOZ_SANDBOX) */
}

#if !defined(XP_WIN) && !defined(XP_MACOSX)
static void
ConvertToUTF8(nsCString& aString)
{
  Unused << UTF_8_ENCODING->DecodeWithoutBOMHandling(aString, aString);
}
#endif

nsresult nsPluginTag::EnsureMembersAreUTF8()
{
#if defined(XP_WIN) || defined(XP_MACOSX)
  return NS_OK;
#else
  ConvertToUTF8(mFileName);
  ConvertToUTF8(mFullPath);
  ConvertToUTF8(mName);
  ConvertToUTF8(mDescription);
  for (uint32_t i = 0; i < mMimeDescriptions.Length(); ++i) {
    ConvertToUTF8(mMimeDescriptions[i]);
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
  return mBlocklistState == nsIBlocklistService::STATE_BLOCKED;
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
  return IsEnabledStateLockedForPlugin(this, aIsEnabledStateLocked);
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
nsPluginTag::GetEnabledState(uint32_t *aEnabledState)
{
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
nsPluginTag::SetEnabledState(uint32_t aEnabledState)
{
  if (aEnabledState >= ePluginState_MaxValue)
    return NS_ERROR_ILLEGAL_VALUE;
  uint32_t oldState = nsIPluginTag::STATE_DISABLED;
  GetEnabledState(&oldState);
  if (oldState != aEnabledState) {
    Preferences::SetInt(GetStatePrefNameForPlugin(this).get(), aEnabledState);
    if (RefPtr<nsPluginHost> host = nsPluginHost::GetInst()) {
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

NS_IMETHODIMP
nsPluginTag::GetLoaded(bool* aIsLoaded)
{
  *aIsLoaded = !!mPlugin;
  return NS_OK;
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

const nsCString&
nsPluginTag::GetNiceFileName()
{
  if (!mNiceFileName.IsEmpty()) {
    return mNiceFileName;
  }

  if (mIsFlashPlugin) {
    mNiceFileName.AssignLiteral("flash");
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

NS_IMETHODIMP
nsPluginTag::GetBlocklistState(uint32_t *aResult)
{
  *aResult = mBlocklistState;
  return NS_OK;
}

void
nsPluginTag::SetBlocklistState(uint32_t aBlocklistState)
{
  mBlocklistState = aBlocklistState;
}

uint32_t
nsPluginTag::BlocklistState()
{
  return mBlocklistState;
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

/* nsFakePluginTag */

uint32_t nsFakePluginTag::sNextId;

nsFakePluginTag::nsFakePluginTag()
  : mId(sNextId++),
    mState(nsPluginTag::ePluginState_Disabled)
{
}

nsFakePluginTag::nsFakePluginTag(uint32_t aId,
                                 already_AddRefed<nsIURI>&& aHandlerURI,
                                 const char* aName,
                                 const char* aDescription,
                                 const nsTArray<nsCString>& aMimeTypes,
                                 const nsTArray<nsCString>& aMimeDescriptions,
                                 const nsTArray<nsCString>& aExtensions,
                                 const nsCString& aNiceName,
                                 const nsString& aSandboxScript)
  : nsIInternalPluginTag(aName, aDescription, nullptr, nullptr,
                         aMimeTypes, aMimeDescriptions, aExtensions),
    mId(aId),
    mHandlerURI(aHandlerURI),
    mNiceName(aNiceName),
    mSandboxScript(aSandboxScript),
    mState(nsPluginTag::ePluginState_Enabled)
{}

nsFakePluginTag::~nsFakePluginTag()
{
}

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
nsresult
nsFakePluginTag::Create(const FakePluginTagInit& aInitDictionary,
                        nsFakePluginTag** aPluginTag)
{
  NS_ENSURE_TRUE(sNextId <= PR_INT32_MAX, NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(!aInitDictionary.mMimeEntries.IsEmpty(), NS_ERROR_INVALID_ARG);

  RefPtr<nsFakePluginTag> tag = new nsFakePluginTag();
  nsresult rv = NS_NewURI(getter_AddRefs(tag->mHandlerURI),
                          aInitDictionary.mHandlerURI);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUTF16toUTF8(aInitDictionary.mNiceName, tag->mNiceName);
  CopyUTF16toUTF8(aInitDictionary.mFullPath, tag->mFullPath);
  CopyUTF16toUTF8(aInitDictionary.mName, tag->mName);
  CopyUTF16toUTF8(aInitDictionary.mDescription, tag->mDescription);
  CopyUTF16toUTF8(aInitDictionary.mFileName, tag->mFileName);
  CopyUTF16toUTF8(aInitDictionary.mVersion, tag->mVersion);
  tag->mSandboxScript = aInitDictionary.mSandboxScript;

  for (const FakePluginMimeEntry& mimeEntry : aInitDictionary.mMimeEntries) {
    CopyUTF16toUTF8(mimeEntry.mType, *tag->mMimeTypes.AppendElement());
    CopyUTF16toUTF8(mimeEntry.mDescription,
                    *tag->mMimeDescriptions.AppendElement());
    CopyUTF16toUTF8(mimeEntry.mExtension, *tag->mExtensions.AppendElement());
  }

  tag.forget(aPluginTag);
  return NS_OK;
}

bool
nsFakePluginTag::HandlerURIMatches(nsIURI* aURI)
{
  bool equals = false;
  return NS_SUCCEEDED(mHandlerURI->Equals(aURI, &equals)) && equals;
}

NS_IMETHODIMP
nsFakePluginTag::GetHandlerURI(nsIURI **aResult)
{
  NS_IF_ADDREF(*aResult = mHandlerURI);
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetSandboxScript(nsAString& aSandboxScript)
{
  aSandboxScript = mSandboxScript;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetDescription(/* utf-8 */ nsACString& aResult)
{
  aResult = mDescription;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetFilename(/* utf-8 */ nsACString& aResult)
{
  aResult = mFileName;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetFullpath(/* utf-8 */ nsACString& aResult)
{
  aResult = mFullPath;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetVersion(/* utf-8 */ nsACString& aResult)
{
  aResult = mVersion;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetName(/* utf-8 */ nsACString& aResult)
{
  aResult = mName;
  return NS_OK;
}

const nsCString&
nsFakePluginTag::GetNiceFileName()
{
  // We don't try to mimic the special-cased flash/java names if the fake plugin
  // claims one of their MIME types, but do allow directly setting niceName if
  // emulating those is desired.
  if (mNiceName.IsEmpty() && !mFileName.IsEmpty()) {
    mNiceName = MakeNiceFileName(mFileName);
  }

  return mNiceName;
}

NS_IMETHODIMP
nsFakePluginTag::GetNiceName(/* utf-8 */ nsACString& aResult)
{
  aResult = GetNiceFileName();
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetBlocklistState(uint32_t* aResult)
{
  // Fake tags don't currently support blocklisting
  *aResult = nsIBlocklistService::STATE_NOT_BLOCKED;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetBlocklisted(bool* aBlocklisted)
{
  // Fake tags can't be blocklisted
  *aBlocklisted = false;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetIsEnabledStateLocked(bool* aIsEnabledStateLocked)
{
  return IsEnabledStateLockedForPlugin(this, aIsEnabledStateLocked);
}

bool
nsFakePluginTag::IsEnabled()
{
  return mState == nsPluginTag::ePluginState_Enabled ||
         mState == nsPluginTag::ePluginState_Clicktoplay;
}

NS_IMETHODIMP
nsFakePluginTag::GetDisabled(bool* aDisabled)
{
  *aDisabled = !IsEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetClicktoplay(bool* aClicktoplay)
{
  *aClicktoplay = (mState == nsPluginTag::ePluginState_Clicktoplay);
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetEnabledState(uint32_t* aEnabledState)
{
  *aEnabledState = (uint32_t)mState;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::SetEnabledState(uint32_t aEnabledState)
{
  // There are static asserts above enforcing that this enum matches
  mState = (nsPluginTag::PluginState)aEnabledState;
  // FIXME-jsplugins update
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetMimeTypes(uint32_t* aCount, char16_t*** aResults)
{
  return CStringArrayToXPCArray(mMimeTypes, aCount, aResults);
}

NS_IMETHODIMP
nsFakePluginTag::GetMimeDescriptions(uint32_t* aCount, char16_t*** aResults)
{
  return CStringArrayToXPCArray(mMimeDescriptions, aCount, aResults);
}

NS_IMETHODIMP
nsFakePluginTag::GetExtensions(uint32_t* aCount, char16_t*** aResults)
{
  return CStringArrayToXPCArray(mExtensions, aCount, aResults);
}

NS_IMETHODIMP
nsFakePluginTag::GetActive(bool *aResult)
{
  // Fake plugins can't be blocklisted, so this is just !Disabled
  *aResult = IsEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetLastModifiedTime(PRTime* aLastModifiedTime)
{
  // FIXME-jsplugins What should this return, if anything?
  MOZ_ASSERT(aLastModifiedTime);
  *aLastModifiedTime = 0;
  return NS_OK;
}

// We don't load fake plugins out of a library, so they should always be there.
NS_IMETHODIMP
nsFakePluginTag::GetLoaded(bool* ret)
{
  *ret = true;
  return NS_OK;
}

NS_IMETHODIMP
nsFakePluginTag::GetId(uint32_t* aId)
{
  *aId = mId;
  return NS_OK;
}
