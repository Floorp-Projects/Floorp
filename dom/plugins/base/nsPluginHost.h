/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginHost_h_
#define nsPluginHost_h_

#include "mozilla/StaticPtr.h"

#include "nsCOMPtr.h"
#include "nsIPluginTag.h"
#include "nsWeakReference.h"
#include "nsTArray.h"
#include "nsPluginTags.h"

class nsPluginHost final : public nsSupportsWeakReference {
  friend class nsFakePluginTag;
  virtual ~nsPluginHost() = default;

 public:
  nsPluginHost() = default;

  static already_AddRefed<nsPluginHost> GetInst();

  NS_DECL_ISUPPORTS

  // Acts like a bitfield
  enum PluginFilter { eExcludeNone, eExcludeDisabled, eExcludeFake };

  NS_IMETHOD GetPluginTagForType(const nsACString& aMimeType,
                                 uint32_t aExcludeFlags,
                                 nsIPluginTag** aResult);

  // FIXME-jsplugins comment about fake
  bool HavePluginForType(const nsACString& aMimeType,
                         PluginFilter aFilter = eExcludeDisabled);

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

  nsTArray<RefPtr<nsFakePluginTag>> mFakePlugins;

  // We need to hold a global ptr to ourselves because we register for
  // two different CIDs for some reason...
  static mozilla::StaticRefPtr<nsPluginHost> sInst;
};

#endif  // nsPluginHost_h_
