/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Components.h"

#ifdef MOZ_WEBRTC
#  include "PeerConnectionImpl.h"

using namespace mozilla;

NS_IMPL_COMPONENT_FACTORY(mozilla::PeerConnectionImpl) {
  return do_AddRef(new PeerConnectionImpl()).downcast<nsISupports>();
}

#endif
