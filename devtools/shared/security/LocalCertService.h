/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LocalCertService_h
#define LocalCertService_h

#include "nsILocalCertService.h"

namespace mozilla {

class LocalCertService final : public nsILocalCertService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOCALCERTSERVICE

  LocalCertService();

private:
  nsresult LoginToKeySlot();
  ~LocalCertService();
};

} // namespace mozilla

#endif // LocalCertService_h
