/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ImageBlocker_h
#define mozilla_image_ImageBlocker_h

#include "nsIContentPolicy.h"

namespace mozilla {
namespace image {

#define IMAGEBLOCKER_CONTRACTID "@mozilla.org/image-blocker-content-policy;1"
#define IMAGEBLOCKER_CID                             \
  { /* f6fcd651-164b-4416-b001-9c8c393fd93b */       \
    0xf6fcd651, 0x164b, 0x4416, {                    \
      0xb0, 0x01, 0x9c, 0x8c, 0x39, 0x3f, 0xd9, 0x3b \
    }                                                \
  }

class ImageBlocker final : public nsIContentPolicy {
  ~ImageBlocker() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPOLICY

  static bool ShouldBlock(nsIURI* aContentLocation);
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_ImageBlocker_h
