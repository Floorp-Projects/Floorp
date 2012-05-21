/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_AStream_h
#define mozilla_plugins_AStream_h

namespace mozilla {
namespace plugins {

/**
 * When we are passed NPStream->{ndata,pdata} in {NPN,NPP}_DestroyStream, we
 * don't know whether it's a plugin stream or a browser stream. This abstract
 * class lets us cast to the right type of object and send the appropriate
 * message.
 */
class AStream
{
public:
  virtual bool IsBrowserStream() = 0;
};

} // namespace plugins
} // namespace mozilla

#endif
