/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* local header for xpconnect tests components */

#ifndef xpctest_private_h___
#define xpctest_private_h___

#include "nsISupports.h"
#include "nsMemory.h"
#include "nsStringGlue.h"
#include "xpctest_attributes.h"
#include "xpctest_params.h"
#include "mozilla/Attributes.h"

class xpcTestObjectReadOnly MOZ_FINAL : public nsIXPCTestObjectReadOnly {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCTESTOBJECTREADONLY
  xpcTestObjectReadOnly();

 private:
    bool    boolProperty;
    int16_t shortProperty;
    int32_t longProperty;
    float   floatProperty;
    char    charProperty;
    PRTime  timeProperty;
};

class xpcTestObjectReadWrite MOZ_FINAL : public nsIXPCTestObjectReadWrite {
  public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCTESTOBJECTREADWRITE

  xpcTestObjectReadWrite();
  ~xpcTestObjectReadWrite();

 private:
     bool boolProperty;
     int16_t shortProperty;
     int32_t longProperty;
     float floatProperty;
     char charProperty;
     char *stringProperty;
     PRTime timeProperty;
};

class nsXPCTestParams MOZ_FINAL : public nsIXPCTestParams
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTPARAMS

    nsXPCTestParams();

private:
    ~nsXPCTestParams();
};

#endif /* xpctest_private_h___ */
