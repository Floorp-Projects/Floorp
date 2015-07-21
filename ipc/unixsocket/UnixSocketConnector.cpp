/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UnixSocketConnector.h"
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR

namespace mozilla {
namespace ipc {

UnixSocketConnector::UnixSocketConnector()
{
  MOZ_COUNT_CTOR(UnixSocketConnector);
}

UnixSocketConnector::~UnixSocketConnector()
{
  MOZ_COUNT_DTOR(UnixSocketConnector);
}

}
}
