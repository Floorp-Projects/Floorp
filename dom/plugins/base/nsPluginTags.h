/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginTags_h_
#define nsPluginTags_h_

#include "nscore.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIPluginTag.h"
#include "nsNPAPIPluginInstance.h"
#include "nsITimer.h"
#include "nsIDOMMimeType.h"

class nsPluginHost;
struct PRLibrary;
struct nsPluginInfo;

// Remember that flags are written out to pluginreg.dat, be careful
// changing their meaning.
#define NS_PLUGIN_FLAG_ENABLED      0x0001    // is this plugin enabled?
// no longer used                   0x0002    // reuse only if regenerating pluginreg.dat
#define NS_PLUGIN_FLAG_FROMCACHE    0x0004    // this plugintag info was loaded from cache
// no longer used                   0x0008    // reuse only if regenerating pluginreg.dat
#define NS_PLUGIN_FLAG_BLOCKLISTED  0x0010    // this is a blocklisted plugin
#define NS_PLUGIN_FLAG_CLICKTOPLAY  0x0020    // this is a click-to-play plugin

// A linked-list of plugin information that is used for instantiating plugins
// and reflecting plugin information into JavaScript.
class nsPluginTag : public nsIPluginTag
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINTAG
  
  nsPluginTag(nsPluginTag* aPluginTag);
  nsPluginTag(nsPluginInfo* aPluginInfo);
  nsPluginTag(const char* aName,
              const char* aDescription,
              const char* aFileName,
              const char* aFullPath,
              const char* aVersion,
              const char* const* aMimeTypes,
              const char* const* aMimeDescriptions,
              const char* const* aExtensions,
              int32_t aVariants,
              int64_t aLastModifiedTime = 0,
              bool aArgsAreUTF8 = false);
  virtual ~nsPluginTag();
  
  void SetHost(nsPluginHost * aHost);
  void TryUnloadPlugin(bool inShutdown);
  void Mark(uint32_t mask);
  void UnMark(uint32_t mask);
  bool HasFlag(uint32_t flag);
  uint32_t Flags();
  bool HasSameNameAndMimes(const nsPluginTag *aPluginTag) const;
  bool IsEnabled();
  
  nsRefPtr<nsPluginTag> mNext;
  nsPluginHost *mPluginHost;
  nsCString     mName; // UTF-8
  nsCString     mDescription; // UTF-8
  nsTArray<nsCString> mMimeTypes; // UTF-8
  nsTArray<nsCString> mMimeDescriptions; // UTF-8
  nsTArray<nsCString> mExtensions; // UTF-8
  PRLibrary     *mLibrary;
  nsRefPtr<nsNPAPIPlugin> mPlugin;
  bool          mIsJavaPlugin;
  bool          mIsFlashPlugin;
  nsCString     mFileName; // UTF-8
  nsCString     mFullPath; // UTF-8
  nsCString     mVersion;  // UTF-8
  int64_t       mLastModifiedTime;
  nsCOMPtr<nsITimer> mUnloadTimer;
private:
  uint32_t      mFlags;

  void InitMime(const char* const* aMimeTypes,
                const char* const* aMimeDescriptions,
                const char* const* aExtensions,
                uint32_t aVariantCount);
  nsresult EnsureMembersAreUTF8();
};

class DOMMimeTypeImpl : public nsIDOMMimeType {
public:
  NS_DECL_ISUPPORTS

  DOMMimeTypeImpl(nsPluginTag* aTag, uint32_t aMimeTypeIndex)
  {
    if (!aTag)
      return;
    CopyUTF8toUTF16(aTag->mMimeDescriptions[aMimeTypeIndex], mDescription);
    CopyUTF8toUTF16(aTag->mExtensions[aMimeTypeIndex], mSuffixes);
    CopyUTF8toUTF16(aTag->mMimeTypes[aMimeTypeIndex], mType);
  }

  virtual ~DOMMimeTypeImpl() {
  }

  NS_METHOD GetDescription(nsAString& aDescription)
  {
    aDescription.Assign(mDescription);
    return NS_OK;
  }

  NS_METHOD GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin)
  {
    // this has to be implemented by the DOM version.
    *aEnabledPlugin = nullptr;
    return NS_OK;
  }

  NS_METHOD GetSuffixes(nsAString& aSuffixes)
  {
    aSuffixes.Assign(mSuffixes);
    return NS_OK;
  }

  NS_METHOD GetType(nsAString& aType)
  {
    aType.Assign(mType);
    return NS_OK;
  }
private:
  nsString mDescription;
  nsString mSuffixes;
  nsString mType;
};

#endif // nsPluginTags_h_
