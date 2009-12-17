// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CRYPTO_CSSM_INIT_H_
#define BASE_CRYPTO_CSSM_INIT_H_

namespace base {

// Initialize CSSM if it isn't already initialized.  This must be called before
// any other CSSM functions.  This function is thread-safe, and CSSM will only
// ever be initialized once.  CSSM will be properly shut down on program exit.
void EnsureCSSMInit();

}  // namespace base

#endif  // BASE_CRYPTO_CSSM_INIT_H_
