/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIGlobalObject.h"
#include "nsContentUtils.h"

nsIPrincipal*
nsIGlobalObject::PrincipalOrNull()
{
  JSObject *global = GetGlobalJSObject();
  if (NS_WARN_IF(!global))
    return nullptr;

  return nsContentUtils::GetObjectPrincipal(global);
}
