/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientPrincipalUtils.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::ContentPrincipalInfo;
using mozilla::ipc::PrincipalInfo;

bool ClientMatchPrincipalInfo(const PrincipalInfo& aLeft,
                              const PrincipalInfo& aRight) {
  if (aLeft.type() != aRight.type()) {
    return false;
  }

  switch (aLeft.type()) {
    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& leftContent =
          aLeft.get_ContentPrincipalInfo();
      const ContentPrincipalInfo& rightContent =
          aRight.get_ContentPrincipalInfo();
      return leftContent.attrs() == rightContent.attrs() &&
             leftContent.originNoSuffix() == rightContent.originNoSuffix();
    }
    case PrincipalInfo::TSystemPrincipalInfo: {
      // system principal always matches
      return true;
    }
    case PrincipalInfo::TNullPrincipalInfo: {
      // null principal never matches
      return false;
    }
    default: {
      break;
    }
  }

  // Clients (windows/workers) should never have an expanded principal type.
  MOZ_CRASH("unexpected principal type!");
}

}  // namespace dom
}  // namespace mozilla
