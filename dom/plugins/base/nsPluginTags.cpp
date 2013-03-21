/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginTags.h"

#include "prlink.h"
#include "plstr.h"
#include "nsIPluginInstanceOwner.h"
#include "nsServiceManagerUtils.h"
#include "nsPluginsDir.h"
#include "nsPluginHost.h"
#include "nsIUnicodeDecoder.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsIDOMMimeType.h"
#include "nsPluginLogging.h"
#include "nsNPAPIPlugin.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Preferences.h"
#include <cctype>

using namespace mozilla;
using mozilla::TimeStamp;

// These legacy flags are used in the plugin registry. The states are now
// stored in prefs, but we still need to be able to import them.
#define NS_PLUGIN_FLAG_ENABLED      0x0001    // is this plugin enabled?
// no longer used                   0x0002    // reuse only if regenerating pluginreg.dat
#define NS_PLUGIN_FLAG_FROMCACHE    0x0004    // this plugintag info was loaded from cache
// no longer used                   0x0008    // reuse only if regenerating pluginreg.dat
#define NS_PLUGIN_FLAG_BLOCKLISTED  0x0010    // this is a blocklisted plugin
#define NS_PLUGIN_FLAG_CLICKTOPLAY  0x0020    // this is a click-to-play plugin

inline char* new_str(const char* str)
{
  if (str == nullptr)
    return nullptr;
  
  char* result = new char[strlen(str) + 1];
  if (result != nullptr)
    return strcpy(result, str);
  return result;
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

static nsCString
GetStatePrefNameForPlugin(nsPluginTag* aTag)
{
  return MakePrefNameForPlugin("state", aTag);
}

static nsCString
GetBlocklistedPrefNameForPlugin(nsPluginTag* aTag)
{
  return MakePrefNameForPlugin("blocklisted", aTag);
}

NS_IMPL_ISUPPORTS1(DOMMimeTypeImpl, nsIDOMMimeType)

/* nsPluginTag */

nsPluginTag::nsPluginTag(nsPluginTag* aPluginTag)
: mPluginHost(nullptr),
mName(aPluginTag->mName),
mDescription(aPluginTag->mDescription),
mMimeTypes(aPluginTag->mMimeTypes),
mMimeDescriptions(aPluginTag->mMimeDescriptions),
mExtensions(aPluginTag->mExtensions),
mLibrary(nullptr),
mIsJavaPlugin(aPluginTag->mIsJavaPlugin),
mIsFlashPlugin(aPluginTag->mIsFlashPlugin),
mFileName(aPluginTag->mFileName),
mFullPath(aPluginTag->mFullPath),
mVersion(aPluginTag->mVersion),
mLastModifiedTime(0),
mNiceFileName()
{
}

nsPluginTag::nsPluginTag(nsPluginInfo* aPluginInfo)
: mPluginHost(nullptr),
mName(aPluginInfo->fName),
mDescription(aPluginInfo->fDescription),
mLibrary(nullptr),
mIsJavaPlugin(false),
mIsFlashPlugin(false),
mFileName(aPluginInfo->fFileName),
mFullPath(aPluginInfo->fFullPath),
mVersion(aPluginInfo->fVersion),
mLastModifiedTime(0),
mNiceFileName()
{
  InitMime(aPluginInfo->fMimeTypeArray,
           aPluginInfo->fMimeDescriptionArray,
           aPluginInfo->fExtensionArray,
           aPluginInfo->fVariantCount);
  EnsureMembersAreUTF8();
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
                         bool aArgsAreUTF8)
: mPluginHost(nullptr),
mName(aName),
mDescription(aDescription),
mLibrary(nullptr),
mIsJavaPlugin(false),
mIsFlashPlugin(false),
mFileName(aFileName),
mFullPath(aFullPath),
mVersion(aVersion),
mLastModifiedTime(aLastModifiedTime),
mNiceFileName()
{
  InitMime(aMimeTypes, aMimeDescriptions, aExtensions, static_cast<uint32_t>(aVariants));
  if (!aArgsAreUTF8)
    EnsureMembersAreUTF8();
}

nsPluginTag::~nsPluginTag()
{
  NS_ASSERTION(!mNext, "Risk of exhausting the stack space, bug 486349");
}

NS_IMPL_ISUPPORTS1(nsPluginTag, nsIPluginTag)

void nsPluginTag::InitMime(const char* const* aMimeTypes,
                           const char* const* aMimeDescriptions,
                           const char* const* aExtensions,
                           uint32_t aVariantCount)
{
  if (!aMimeTypes) {
    return;
  }

  for (uint32_t i = 0; i < aVariantCount; i++) {
    if (!aMimeTypes[i] || !nsPluginHost::IsTypeWhitelisted(aMimeTypes[i])) {
      continue;
    }

    // Look for certain special plugins.
    if (nsPluginHost::IsJavaMIMEType(aMimeTypes[i])) {
      mIsJavaPlugin = true;
    } else if (strcmp(aMimeTypes[i], "application/x-shockwave-flash") == 0) {
      mIsFlashPlugin = true;
    }

    // Fill in our MIME type array.
    mMimeTypes.AppendElement(nsCString(aMimeTypes[i]));

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
  if (!EnsureStringLength(buffer, outUnicodeLen))
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
  nsCOMPtr<nsICharsetConverterManager> ccm =
  do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsAutoCString charset;
  rv = pcs->GetCharset(kPlatformCharsetSel_FileName, charset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!charset.LowerCaseEqualsLiteral("utf-8")) {
    rv = ccm->GetUnicodeDecoderRaw(charset.get(), getter_AddRefs(decoder));
    NS_ENSURE_SUCCESS(rv, rv);
    
    ConvertToUTF8(decoder, mFileName);
    ConvertToUTF8(decoder, mFullPath);
  }
  
  // The description of the plug-in and the various MIME type descriptions
  // should be encoded in the standard plain text file encoding for this system.
  // XXX should we add kPlatformCharsetSel_PluginResource?
  rv = pcs->GetCharset(kPlatformCharsetSel_PlainTextInFile, charset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!charset.LowerCaseEqualsLiteral("utf-8")) {
    rv = ccm->GetUnicodeDecoderRaw(charset.get(), getter_AddRefs(decoder));
    NS_ENSURE_SUCCESS(rv, rv);
    
    ConvertToUTF8(decoder, mName);
    ConvertToUTF8(decoder, mDescription);
    for (uint32_t i = 0; i < mMimeDescriptions.Length(); ++i) {
      ConvertToUTF8(decoder, mMimeDescriptions[i]);
    }
  }
  return NS_OK;
#endif
}

void nsPluginTag::SetHost(nsPluginHost * aHost)
{
  mPluginHost = aHost;
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

bool
nsPluginTag::IsEnabled()
{
  const PluginState state = GetPluginState();
  return (state == ePluginState_Enabled) || (state == ePluginState_Clicktoplay);
}

void
nsPluginTag::SetEnabled(bool enabled)
{
  if (enabled == IsEnabled()) {
    return;
  }

  PluginState state = GetPluginState();

  if (!enabled) {
    SetPluginState(ePluginState_Disabled);
  } else if (state != ePluginState_Clicktoplay) {
    SetPluginState(ePluginState_Enabled);
  }

  mPluginHost->UpdatePluginInfo(this);
}

NS_IMETHODIMP
nsPluginTag::GetDisabled(bool* aDisabled)
{
  *aDisabled = !IsEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::SetDisabled(bool aDisabled)
{
  SetEnabled(!aDisabled);
  return NS_OK;
}

bool
nsPluginTag::IsBlocklisted()
{
  return Preferences::GetBool(GetBlocklistedPrefNameForPlugin(this).get(), false);
}

NS_IMETHODIMP
nsPluginTag::GetBlocklisted(bool* aBlocklisted)
{
  *aBlocklisted = IsBlocklisted();
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::SetBlocklisted(bool blocklisted)
{
  if (blocklisted == IsBlocklisted()) {
    return NS_OK;
  }

  const nsCString pref = GetBlocklistedPrefNameForPlugin(this);
  if (blocklisted) {
    Preferences::SetBool(pref.get(), true);
  } else {
    Preferences::ClearUser(pref.get());
  }

  mPluginHost->UpdatePluginInfo(this);
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
nsPluginTag::SetClicktoplay(bool clicktoplay)
{
  if (clicktoplay == IsClicktoplay()) {
    return NS_OK;
  }

  const PluginState state = GetPluginState();
  if (state != ePluginState_Disabled) {
    SetPluginState(ePluginState_Clicktoplay);
  }

  mPluginHost->UpdatePluginInfo(this);
  return NS_OK;
}

nsPluginTag::PluginState
nsPluginTag::GetPluginState()
{
  return (PluginState)Preferences::GetInt(GetStatePrefNameForPlugin(this).get(),
                                          ePluginState_Enabled);
}

void
nsPluginTag::SetPluginState(PluginState state)
{
  Preferences::SetInt(GetStatePrefNameForPlugin(this).get(), state);
}

NS_IMETHODIMP
nsPluginTag::GetMimeTypes(uint32_t* aCount, nsIDOMMimeType*** aResults)
{
  uint32_t count = mMimeTypes.Length();
  *aResults = static_cast<nsIDOMMimeType**>
                         (nsMemory::Alloc(count * sizeof(**aResults)));
  if (!*aResults)
    return NS_ERROR_OUT_OF_MEMORY;
  *aCount = count;

  for (uint32_t i = 0; i < count; i++) {
    nsIDOMMimeType* mimeType = new DOMMimeTypeImpl(this, i);
    (*aResults)[i] = mimeType;
    NS_ADDREF((*aResults)[i]);
  }

  return NS_OK;
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
  if (mLibrary && !inShutdown) {
    return;
  }

  if (mPlugin) {
    mPlugin->Shutdown();
    mPlugin = nullptr;
  }
}

nsCString nsPluginTag::GetNiceFileName() {
  if (!mNiceFileName.IsEmpty()) {
    return mNiceFileName;
  }

  if (mIsFlashPlugin) {
    mNiceFileName.Assign(NS_LITERAL_CSTRING("flash"));
    return mNiceFileName;
  }

  if (mIsJavaPlugin) {
    mNiceFileName.Assign(NS_LITERAL_CSTRING("java"));
    return mNiceFileName;
  }

  mNiceFileName.Assign(mFileName);
  int32_t niceNameLength = mFileName.RFind(".");
  NS_ASSERTION(niceNameLength != kNotFound, "mFileName doesn't have a '.'?");
  while (niceNameLength > 0) {
    char chr = mFileName[niceNameLength - 1];
    if (!std::isalpha(chr))
      niceNameLength--;
    else
      break;
  }

  // If it turns out that niceNameLength <= 0, we'll fall back and use the
  // entire mFileName (which we've already taken care of, a few lines back)
  if (niceNameLength > 0) {
    mNiceFileName.Truncate(niceNameLength);
  }

  ToLowerCase(mNiceFileName);
  return mNiceFileName;
}

void nsPluginTag::ImportFlagsToPrefs(uint32_t flags)
{
  if (!(flags & NS_PLUGIN_FLAG_ENABLED)) {
    SetPluginState(ePluginState_Disabled);
  }

  if (flags & NS_PLUGIN_FLAG_BLOCKLISTED) {
    Preferences::SetBool(GetBlocklistedPrefNameForPlugin(this).get(), true);
  }
}
