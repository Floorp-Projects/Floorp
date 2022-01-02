/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientValidation_h
#define _mozilla_dom_ClientValidation_h

#include "nsStringFwd.h"

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}  // namespace ipc

namespace dom {

bool ClientIsValidPrincipalInfo(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

bool ClientIsValidCreationURL(const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                              const nsACString& aURL);

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ClientValidation_h
