/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* describes principals by their orginating uris*/

#ifndef nsJSPrincipals_h__
#define nsJSPrincipals_h__
#include "jsapi.h"
#include "nsIPrincipal.h"

class nsCString;

struct nsJSPrincipals : nsIPrincipal, JSPrincipals
{
  static JSBool Subsume(JSPrincipals *jsprin, JSPrincipals *other);
  static void Destroy(JSPrincipals *jsprin);

  /*
   * Get a weak reference to nsIPrincipal associated with the given JS
   * principal.
   */
  static nsJSPrincipals* get(JSPrincipals *principals) {
    nsJSPrincipals *self = static_cast<nsJSPrincipals *>(principals);
    MOZ_ASSERT_IF(self, self->debugToken == DEBUG_TOKEN);
    return self;
  }
  
  static nsJSPrincipals* get(nsIPrincipal *principal) {
    nsJSPrincipals *self = static_cast<nsJSPrincipals *>(principal);
    MOZ_ASSERT_IF(self, self->debugToken == DEBUG_TOKEN);
    return self;
  }

  nsJSPrincipals() {
    refcount = 0;
    setDebugToken(DEBUG_TOKEN);
  }

  virtual ~nsJSPrincipals() {
    setDebugToken(0);
  }

  /**
   * Return a string that can be used as JS script filename in error reports.
   */
  virtual void GetScriptLocation(nsACString &aStr) = 0;

#ifdef DEBUG
  virtual void dumpImpl() = 0;
#endif

  static const uint32_t DEBUG_TOKEN = 0x0bf41760;
};

#endif /* nsJSPrincipals_h__ */
