/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SECURITY_TRUSTED_TYPES_TRUSTEDSCRIPT_H_
#define DOM_SECURITY_TRUSTED_TYPES_TRUSTEDSCRIPT_H_

#include "mozilla/dom/TrustedTypeUtils.h"

namespace mozilla::dom {

// https://w3c.github.io/trusted-types/dist/spec/#trusted-script
DECL_TRUSTED_TYPE_CLASS(TrustedScript)

}  // namespace mozilla::dom

#endif  // DOM_SECURITY_TRUSTED_TYPES_TRUSTEDSCRIPT_H_
