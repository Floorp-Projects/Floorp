/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPCallbackBase_h_
#define GMPCallbackBase_h_

class GMPCallbackBase
{
public:
  virtual ~GMPCallbackBase() {}

  // The GMP code will call this if the codec crashes or shuts down.  It's
  // expected that the consumer (destination of this callback) will respond
  // by dropping their reference to the proxy, allowing the proxy/parent to
  // be destroyed.
  virtual void Terminated() = 0;
};

#endif
