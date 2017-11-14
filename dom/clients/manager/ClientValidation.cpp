/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientValidation.h"

#include "mozilla/net/MozURL.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::ContentPrincipalInfo;
using mozilla::ipc::PrincipalInfo;
using mozilla::net::MozURL;

bool
ClientIsValidPrincipalInfo(const PrincipalInfo& aPrincipalInfo)
{
  // Ideally we would verify that the source process has permission to
  // create a window or worker with the given principal, but we don't
  // currently have any such restriction in place.  Instead, at least
  // verify the PrincipalInfo is an expected type and has a parsable
  // origin/spec.
  switch (aPrincipalInfo.type()) {
    // Any system and null principal is acceptable.
    case PrincipalInfo::TSystemPrincipalInfo:
    case PrincipalInfo::TNullPrincipalInfo:
    {
      return true;
    }

    // Validate content principals to ensure that the origin and spec are sane.
    case PrincipalInfo::TContentPrincipalInfo:
    {
      const ContentPrincipalInfo& content =
       aPrincipalInfo.get_ContentPrincipalInfo();

      // Verify the principal spec parses.
      RefPtr<MozURL> specURL;
      nsresult rv = MozURL::Init(getter_AddRefs(specURL), content.spec());
      NS_ENSURE_SUCCESS(rv, false);

      // Verify the principal originNoSuffix parses.
      RefPtr<MozURL> originURL;
      rv = MozURL::Init(getter_AddRefs(originURL),
                        content.originNoSuffix().get_nsCString());
      NS_ENSURE_SUCCESS(rv, false);

      nsAutoCString originOrigin;
      rv = originURL->GetOrigin(originOrigin);
      NS_ENSURE_SUCCESS(rv, false);

      nsAutoCString specOrigin;
      rv = specURL->GetOrigin(specOrigin);
      NS_ENSURE_SUCCESS(rv, false);

      // For now require Clients to have a principal where both its
      // originNoSuffix and spec have the same origin.  This will
      // exclude a variety of unusual combinations within the browser
      // but its adequate for the features need to support right now.
      // If necessary we could expand this function to handle more
      // cases in the future.
      return specOrigin == originOrigin;
    }
    default:
    {
      break;
    }
  }

  // Windows and workers should not have expanded URLs, etc.
  return false;
}

} // namespace dom
} // namespace mozilla
