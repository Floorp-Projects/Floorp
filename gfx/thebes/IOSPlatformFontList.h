/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IOSPlatformFontList_H_
#define IOSPlatformFontList_H_

#include <CoreFoundation/CoreFoundation.h>

#include "CoreTextFontList.h"

class IOSPlatformFontList final : public CoreTextFontList {
 public:
  static IOSPlatformFontList* PlatformFontList() {
    return static_cast<IOSPlatformFontList*>(
        gfxPlatformFontList::PlatformFontList());
  }

  static void LookupSystemFont(mozilla::LookAndFeel::FontID aSystemFontID,
                               nsACString& aSystemFontName,
                               gfxFontStyle& aFontStyle);

 protected:
  nsTArray<std::pair<const char**, uint32_t>> GetFilteredPlatformFontLists()
      override;

 private:
  friend class gfxPlatformMac;

  // Only the friend class gfxPlatformMac constructs this.
  IOSPlatformFontList();
  virtual ~IOSPlatformFontList();
};

#endif /* IOSPlatformFontList_H_ */
