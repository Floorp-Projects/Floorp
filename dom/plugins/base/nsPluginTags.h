/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginTags_h_
#define nsPluginTags_h_

#include "mozilla/Attributes.h"
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

// A linked-list of plugin information that is used for instantiating plugins
// and reflecting plugin information into JavaScript.
class nsPluginTag : public nsIPluginTag
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINTAG

  // These must match the STATE_* values in nsIPluginTag.idl
  enum PluginState {
    ePluginState_Disabled = 0,
    ePluginState_Clicktoplay = 1,
    ePluginState_Enabled = 2,
    ePluginState_MaxValue = 3,
  };

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

  void TryUnloadPlugin(bool inShutdown);

  // plugin is enabled and not blocklisted
  bool IsActive();

  bool IsEnabled();
  void SetEnabled(bool enabled);
  bool IsClicktoplay();
  bool IsBlocklisted();

  PluginState GetPluginState();
  void SetPluginState(PluginState state);

  // import legacy flags from plugin registry into the preferences
  void ImportFlagsToPrefs(uint32_t flag);

  bool HasSameNameAndMimes(const nsPluginTag *aPluginTag) const;
  nsCString GetNiceFileName();

  nsRefPtr<nsPluginTag> mNext;
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
  nsCString     mNiceFileName; // UTF-8

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

  NS_METHOD GetDescription(nsAString& aDescription) MOZ_OVERRIDE
  {
    aDescription.Assign(mDescription);
    return NS_OK;
  }

  NS_METHOD GetEnabledPlugin(nsIDOMPlugin** aEnabledPlugin) MOZ_OVERRIDE
  {
    // this has to be implemented by the DOM version.
    *aEnabledPlugin = nullptr;
    return NS_OK;
  }

  NS_METHOD GetSuffixes(nsAString& aSuffixes) MOZ_OVERRIDE
  {
    aSuffixes.Assign(mSuffixes);
    return NS_OK;
  }

  NS_METHOD GetType(nsAString& aType) MOZ_OVERRIDE
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
