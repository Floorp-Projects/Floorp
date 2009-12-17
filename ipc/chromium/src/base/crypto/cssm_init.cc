// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/cssm_init.h"

#include <Security/cssm.h>

#include "base/logging.h"
#include "base/singleton.h"

// When writing crypto code for Mac OS X, you may find the following
// documentation useful:
// - Common Security: CDSA and CSSM, Version 2 (with corrigenda)
//   http://www.opengroup.org/security/cdsa.htm
// - Apple Cryptographic Service Provider Functional Specification
// - CryptoSample: http://developer.apple.com/SampleCode/CryptoSample/

namespace {

class CSSMInitSingleton {
 public:
  CSSMInitSingleton() : inited_(false), loaded_(false) {
    static CSSM_VERSION version = {2, 0};
    // TODO(wtc): what should our caller GUID be?
    static const CSSM_GUID test_guid = {
      0xFADE, 0, 0, { 1, 2, 3, 4, 5, 6, 7, 0 }
    };
    CSSM_RETURN crtn;
    CSSM_PVC_MODE pvc_policy = CSSM_PVC_NONE;
    crtn = CSSM_Init(&version, CSSM_PRIVILEGE_SCOPE_NONE, &test_guid,
                     CSSM_KEY_HIERARCHY_NONE, &pvc_policy, NULL);
    if (crtn) {
      NOTREACHED();
      return;
    }
    inited_ = true;

    crtn = CSSM_ModuleLoad(&gGuidAppleCSP, CSSM_KEY_HIERARCHY_NONE, NULL, NULL);
    if (crtn) {
      NOTREACHED();
      return;
    }
    loaded_ = true;
  }

  ~CSSMInitSingleton() {
    CSSM_RETURN crtn;
    if (loaded_) {
      crtn = CSSM_ModuleUnload(&gGuidAppleCSP, NULL, NULL);
      DCHECK(crtn == CSSM_OK);
    }
    if (inited_) {
      crtn = CSSM_Terminate();
      DCHECK(crtn == CSSM_OK);
    }
  }

 private:
  bool inited_;  // True if CSSM_Init has been called successfully.
  bool loaded_;  // True if CSSM_ModuleLoad has been called successfully.
};

}  // namespace

namespace base {

void EnsureCSSMInit() {
  Singleton<CSSMInitSingleton>::get();
}

}  // namespace base
