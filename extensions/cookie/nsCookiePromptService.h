/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCookiePromptService_h__
#define nsCookiePromptService_h__

#include "nsICookiePromptService.h"

class nsCookiePromptService : public nsICookiePromptService {

  virtual ~nsCookiePromptService();

public:

  nsCookiePromptService();

  NS_DECL_NSICOOKIEPROMPTSERVICE
  NS_DECL_ISUPPORTS

private:

};

// {CE002B28-92B7-4701-8621-CC925866FB87}
#define NS_COOKIEPROMPTSERVICE_CID \
 {0xCE002B28, 0x92B7, 0x4701, {0x86, 0x21, 0xCC, 0x92, 0x58, 0x66, 0xFB, 0x87}}

#endif
