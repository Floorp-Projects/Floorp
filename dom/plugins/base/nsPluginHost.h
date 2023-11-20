/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginHost_h_
#define nsPluginHost_h_

#include "nsStringFwd.h"
#include "nsWeakReference.h"

class nsPluginHost final : public nsSupportsWeakReference {
  virtual ~nsPluginHost() = default;

 public:
  nsPluginHost() = default;

  NS_DECL_ISUPPORTS

  // checks whether aType is a type we recognize for potential special handling
  enum SpecialType {
    eSpecialType_None,
    // Needed to whitelist for async init support
    eSpecialType_Test,
    // Informs some decisions about OOP and quirks
    eSpecialType_Flash
  };
  static SpecialType GetSpecialType(const nsACString& aMIMEType);
};

#endif  // nsPluginHost_h_
