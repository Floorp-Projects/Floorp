/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPluginTags.h"

#include "prlink.h"
#include "plstr.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIDocument.h"
#include "nsServiceManagerUtils.h"
#include "nsPluginsDir.h"
#include "nsPluginHost.h"
#include "nsIUnicodeDecoder.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsPluginLogging.h"
#include "nsICategoryManager.h"
#include "nsNPAPIPlugin.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using mozilla::TimeStamp;

inline char* new_str(const char* str)
{
  if (str == nsnull)
    return nsnull;
  
  char* result = new char[strlen(str) + 1];
  if (result != nsnull)
    return strcpy(result, str);
  return result;
}

/* nsPluginTag */

nsPluginTag::nsPluginTag(nsPluginTag* aPluginTag)
: mPluginHost(nsnull),
mName(aPluginTag->mName),
mDescription(aPluginTag->mDescription),
mMimeTypes(aPluginTag->mMimeTypes),
mMimeDescriptions(aPluginTag->mMimeDescriptions),
mExtensions(aPluginTag->mExtensions),
mLibrary(nsnull),
mIsJavaPlugin(aPluginTag->mIsJavaPlugin),
mIsFlashPlugin(aPluginTag->mIsFlashPlugin),
mFileName(aPluginTag->mFileName),
mFullPath(aPluginTag->mFullPath),
mVersion(aPluginTag->mVersion),
mLastModifiedTime(0),
mFlags(NS_PLUGIN_FLAG_ENABLED)
{
}

nsPluginTag::nsPluginTag(nsPluginInfo* aPluginInfo)
: mPluginHost(nsnull),
mName(aPluginInfo->fName),
mDescription(aPluginInfo->fDescription),
mLibrary(nsnull),
mIsJavaPlugin(false),
mIsFlashPlugin(false),
mFileName(aPluginInfo->fFileName),
mFullPath(aPluginInfo->fFullPath),
mVersion(aPluginInfo->fVersion),
mLastModifiedTime(0),
mFlags(NS_PLUGIN_FLAG_ENABLED)
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
                         PRInt32 aVariants,
                         PRInt64 aLastModifiedTime,
                         bool aArgsAreUTF8)
: mPluginHost(nsnull),
mName(aName),
mDescription(aDescription),
mLibrary(nsnull),
mIsJavaPlugin(false),
mIsFlashPlugin(false),
mFileName(aFileName),
mFullPath(aFullPath),
mVersion(aVersion),
mLastModifiedTime(aLastModifiedTime),
mFlags(0) // Caller will read in our flags from cache
{
  InitMime(aMimeTypes, aMimeDescriptions, aExtensions, static_cast<PRUint32>(aVariants));
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
                           PRUint32 aVariantCount)
{
  if (!aMimeTypes) {
    return;
  }

  for (PRUint32 i = 0; i < aVariantCount; i++) {
    if (!aMimeTypes[i]) {
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
  PRInt32 numberOfBytes = aString.Length();
  PRInt32 outUnicodeLen;
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
  
  nsCAutoString charset;
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
    for (PRUint32 i = 0; i < mMimeDescriptions.Length(); ++i) {
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

NS_IMETHODIMP
nsPluginTag::GetDisabled(bool* aDisabled)
{
  *aDisabled = !HasFlag(NS_PLUGIN_FLAG_ENABLED);
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::SetDisabled(bool aDisabled)
{
  if (HasFlag(NS_PLUGIN_FLAG_ENABLED) == !aDisabled)
    return NS_OK;
  
  if (aDisabled)
    UnMark(NS_PLUGIN_FLAG_ENABLED);
  else
    Mark(NS_PLUGIN_FLAG_ENABLED);
  
  mPluginHost->UpdatePluginInfo(this);
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::GetBlocklisted(bool* aBlocklisted)
{
  *aBlocklisted = HasFlag(NS_PLUGIN_FLAG_BLOCKLISTED);
  return NS_OK;
}

NS_IMETHODIMP
nsPluginTag::SetBlocklisted(bool aBlocklisted)
{
  if (HasFlag(NS_PLUGIN_FLAG_BLOCKLISTED) == aBlocklisted)
    return NS_OK;
  
  if (aBlocklisted)
    Mark(NS_PLUGIN_FLAG_BLOCKLISTED);
  else
    UnMark(NS_PLUGIN_FLAG_BLOCKLISTED);
  
  mPluginHost->UpdatePluginInfo(nsnull);
  return NS_OK;
}

void
nsPluginTag::RegisterWithCategoryManager(bool aOverrideInternalTypes,
                                         nsPluginTag::nsRegisterType aType)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
             ("nsPluginTag::RegisterWithCategoryManager plugin=%s, removing = %s\n",
              mFileName.get(), aType == ePluginUnregister ? "yes" : "no"));
  
  nsCOMPtr<nsICategoryManager> catMan = do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan)
    return;
  
  const char *contractId = "@mozilla.org/content/plugin/document-loader-factory;1";
  
  // A preference controls whether or not the full page plugin is disabled for
  // a particular type. The string must be in the form:
  //   type1,type2,type3,type4
  // Note: need an actual interface to control this and subsequent disabling 
  // (and other plugin host settings) so applications can reliably disable 
  // plugins - without relying on implementation details such as prefs/category
  // manager entries.
  nsCAutoString overrideTypesFormatted;
  if (aType != ePluginUnregister) {
    overrideTypesFormatted.Assign(',');
    nsAdoptingCString overrideTypes =
      Preferences::GetCString("plugin.disable_full_page_plugin_for_types");
    overrideTypesFormatted += overrideTypes;
    overrideTypesFormatted.Append(',');
  }
  
  nsACString::const_iterator start, end;
  for (PRUint32 i = 0; i < mMimeTypes.Length(); i++) {
    if (aType == ePluginUnregister) {
      nsXPIDLCString value;
      if (NS_SUCCEEDED(catMan->GetCategoryEntry("Gecko-Content-Viewers",
                                                mMimeTypes[i].get(),
                                                getter_Copies(value)))) {
        // Only delete the entry if a plugin registered for it
        if (strcmp(value, contractId) == 0) {
          catMan->DeleteCategoryEntry("Gecko-Content-Viewers",
                                      mMimeTypes[i].get(),
                                      true);
        }
      }
    } else {
      overrideTypesFormatted.BeginReading(start);
      overrideTypesFormatted.EndReading(end);
      
      nsCAutoString commaSeparated; 
      commaSeparated.Assign(',');
      commaSeparated += mMimeTypes[i];
      commaSeparated.Append(',');
      if (!FindInReadable(commaSeparated, start, end)) {
        catMan->AddCategoryEntry("Gecko-Content-Viewers",
                                 mMimeTypes[i].get(),
                                 contractId,
                                 false, /* persist: broken by bug 193031 */
                                 aOverrideInternalTypes, /* replace if we're told to */
                                 nsnull);
      }
    }
    
    PLUGIN_LOG(PLUGIN_LOG_NOISY,
               ("nsPluginTag::RegisterWithCategoryManager mime=%s, plugin=%s\n",
                mMimeTypes[i].get(), mFileName.get()));
  }
}

void nsPluginTag::Mark(PRUint32 mask)
{
  bool wasEnabled = IsEnabled();
  mFlags |= mask;
  // Update entries in the category manager if necessary.
  if (mPluginHost && wasEnabled != IsEnabled()) {
    if (wasEnabled)
      RegisterWithCategoryManager(false, nsPluginTag::ePluginUnregister);
    else
      RegisterWithCategoryManager(false, nsPluginTag::ePluginRegister);
  }
}

void nsPluginTag::UnMark(PRUint32 mask)
{
  bool wasEnabled = IsEnabled();
  mFlags &= ~mask;
  // Update entries in the category manager if necessary.
  if (mPluginHost && wasEnabled != IsEnabled()) {
    if (wasEnabled)
      RegisterWithCategoryManager(false, nsPluginTag::ePluginUnregister);
    else
      RegisterWithCategoryManager(false, nsPluginTag::ePluginRegister);
  }
}

bool nsPluginTag::HasFlag(PRUint32 flag)
{
  return (mFlags & flag) != 0;
}

PRUint32 nsPluginTag::Flags()
{
  return mFlags;
}

bool nsPluginTag::IsEnabled()
{
  return HasFlag(NS_PLUGIN_FLAG_ENABLED) && !HasFlag(NS_PLUGIN_FLAG_BLOCKLISTED);
}

bool nsPluginTag::Equals(nsPluginTag *aPluginTag)
{
  NS_ENSURE_TRUE(aPluginTag, false);
  
  if ((!mName.Equals(aPluginTag->mName)) ||
      (!mDescription.Equals(aPluginTag->mDescription)) ||
      (mMimeTypes.Length() != aPluginTag->mMimeTypes.Length())) {
    return false;
  }

  for (PRUint32 i = 0; i < mMimeTypes.Length(); i++) {
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
    mPlugin = nsnull;
  }
}
