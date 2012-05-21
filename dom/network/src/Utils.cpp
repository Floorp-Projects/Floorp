/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Utils.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace dom {
namespace network {

/* extern */ bool
IsAPIEnabled()
{
  return Preferences::GetBool("dom.network.enabled", true);
}

} // namespace network
} // namespace dom
} // namespace mozilla
