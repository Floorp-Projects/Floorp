// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nss_init.h"

#include <nss.h>
#include <plarena.h>
#include <prerror.h>
#include <prinit.h>

// Work around https://bugzilla.mozilla.org/show_bug.cgi?id=455424
// until NSS 3.12.2 comes out and we update to it.
#define Lock FOO_NSS_Lock
#include <secmod.h>
#include <ssl.h>
#undef Lock

#include "base/file_util.h"
#include "base/logging.h"
#include "base/singleton.h"

namespace {

// Load nss's built-in root certs.
SECMODModule *InitDefaultRootCerts() {
  const char* kModulePath = "libnssckbi.so";
  char modparams[1024];
  snprintf(modparams, sizeof(modparams),
          "name=\"Root Certs\" library=\"%s\"", kModulePath);
  SECMODModule *root = SECMOD_LoadUserModule(modparams, NULL, PR_FALSE);
  if (root)
    return root;

  // Aw, snap.  Can't find/load root cert shared library.
  // This will make it hard to talk to anybody via https.
  NOTREACHED();
  return NULL;
}

class NSSInitSingleton {
 public:
  NSSInitSingleton() {
    // Initialize without using a persistant database (e.g. ~/.netscape)
    SECStatus status = NSS_NoDB_Init(".");
    if (status != SECSuccess) {
      char buffer[513] = "Couldn't retrieve error";
      int32_t err_length = PR_GetErrorTextLength();
      if (err_length > 0 && static_cast<size_t>(err_length) < sizeof(buffer))
        PR_GetErrorText(buffer);

      NOTREACHED() << "Error calling NSS_NoDB_Init: " << buffer;
    }

    root_ = InitDefaultRootCerts();

    NSS_SetDomesticPolicy();

    // Explicitly enable exactly those ciphers with keys of at least 80 bits
    for (int i = 0; i < SSL_NumImplementedCiphers; i++) {
      SSLCipherSuiteInfo info;
      if (SSL_GetCipherSuiteInfo(SSL_ImplementedCiphers[i], &info,
                                 sizeof(info)) == SECSuccess) {
        SSL_CipherPrefSetDefault(SSL_ImplementedCiphers[i],
                                 (info.effectiveKeyBits >= 80));
      }
    }

    // Enable SSL
    SSL_OptionSetDefault(SSL_SECURITY, PR_TRUE);

    // All other SSL options are set per-session by SSLClientSocket.
  }

  ~NSSInitSingleton() {
    if (root_) {
      SECMOD_UnloadUserModule(root_);
      SECMOD_DestroyModule(root_);
      root_ = NULL;
    }

    // Have to clear the cache, or NSS_Shutdown fails with SEC_ERROR_BUSY
    SSL_ClearSessionCache();

    SECStatus status = NSS_Shutdown();
    if (status != SECSuccess)
      LOG(ERROR) << "NSS_Shutdown failed, leak?  See "
                    "http://code.google.com/p/chromium/issues/detail?id=4609";

    PL_ArenaFinish();

    PRStatus prstatus = PR_Cleanup();
    if (prstatus != PR_SUCCESS)
      LOG(ERROR) << "PR_Cleanup failed?";
  }

 private:
  SECMODModule *root_;
};

}  // namespace

namespace base {

void EnsureNSSInit() {
  Singleton<NSSInitSingleton>::get();
}

}  // namespace base
