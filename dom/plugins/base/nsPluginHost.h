/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginHost_h_
#define nsPluginHost_h_

#include "mozilla/StaticPtr.h"

#include "nsCOMPtr.h"
#include "prlink.h"
#include "nsIPluginTag.h"
#include "nsWeakReference.h"
#include "nsTArray.h"
#include "nsPluginTags.h"

class nsIFile;

class nsPluginHost final : public nsSupportsWeakReference {
  friend class nsFakePluginTag;
  virtual ~nsPluginHost();

 public:
  nsPluginHost();

  static already_AddRefed<nsPluginHost> GetInst();

  NS_DECL_ISUPPORTS

  // Acts like a bitfield
  enum PluginFilter { eExcludeNone, eExcludeDisabled, eExcludeFake };

  NS_IMETHOD GetPluginTagForType(const nsACString& aMimeType,
                                 uint32_t aExcludeFlags,
                                 nsIPluginTag** aResult);
  NS_IMETHOD GetPermissionStringForTag(nsIPluginTag* aTag,
                                       uint32_t aExcludeFlags,
                                       nsACString& aPermissionString);

  // FIXME-jsplugins comment about fake
  bool HavePluginForType(const nsACString& aMimeType,
                         PluginFilter aFilter = eExcludeDisabled);

  void GetPlugins(nsTArray<nsCOMPtr<nsIInternalPluginTag>>& aPluginArray,
                  bool aIncludeDisabled = false);

  /**
   * Returns true if a plugin can be used to load the requested MIME type. Used
   * for short circuiting before sending things to plugin code.
   */
  static bool CanUsePluginForMIMEType(const nsACString& aMIMEType);

  // checks whether aType is a type we recognize for potential special handling
  enum SpecialType {
    eSpecialType_None,
    // Needed to whitelist for async init support
    eSpecialType_Test,
    // Informs some decisions about OOP and quirks
    eSpecialType_Flash
  };
  static SpecialType GetSpecialType(const nsACString& aMIMEType);

 private:
  // Find a plugin for the given type.  If aIncludeFake is true a fake plugin
  // will be preferred if one exists; otherwise a fake plugin will never be
  // returned.  If aCheckEnabled is false, disabled plugins can be returned.
  nsIInternalPluginTag* FindPluginForType(const nsACString& aMimeType,
                                          bool aIncludeFake,
                                          bool aCheckEnabled);

  // Find specifically a fake plugin for the given type.  If aCheckEnabled is
  // false, disabled plugins can be returned.
  nsFakePluginTag* FindFakePluginForType(const nsACString& aMimeType,
                                         bool aCheckEnabled);

  // Find specifically a fake plugin for the given extension.  If aCheckEnabled
  // is false, disabled plugins can be returned.  aMimeType will be filled in
  // with the MIME type the plugin is registered for.
  nsFakePluginTag* FindFakePluginForExtension(const nsACString& aExtension,
                                              /* out */ nsACString& aMimeType,
                                              bool aCheckEnabled);

  // Checks to see if a tag object is in our list of live tags.
  bool IsLiveTag(nsIPluginTag* tag);

  // To be used by the chrome process whenever the set of plugins changes.
  void IncrementChromeEpoch();

  // To be used by the chrome process; returns the current epoch.
  uint32_t ChromeEpoch();

  // To be used by the content process to get/set the last observed epoch value
  // from the chrome process.
  uint32_t ChromeEpochForContent();
  void SetChromeEpochForContent(uint32_t aEpoch);

  nsTArray<RefPtr<nsFakePluginTag>> mFakePlugins;

  // This epoch increases each time we load the list of plugins from disk.
  // In the chrome process, this stores the actual epoch.
  // In the content process, this stores the last epoch value observed
  // when reading plugins from chrome.
  uint32_t mPluginEpoch;

  // We need to hold a global ptr to ourselves because we register for
  // two different CIDs for some reason...
  static mozilla::StaticRefPtr<nsPluginHost> sInst;
};

#endif  // nsPluginHost_h_
