/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef GeoclueLocationProvider_h
#define GeoclueLocationProvider_h

#include "mozilla/RefPtr.h"
#include "nsIGeolocationProvider.h"

namespace mozilla::dom {

class GCLocProviderPriv;

class GeoclueLocationProvider final : public nsIGeolocationProvider {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONPROVIDER

  GeoclueLocationProvider();

 private:
  ~GeoclueLocationProvider() = default;

  RefPtr<GCLocProviderPriv> mPriv;
};

}  // namespace mozilla::dom

#endif /* GeoclueLocationProvider_h */
