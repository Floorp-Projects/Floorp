/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sts=2 ts=8 sw=2 tw=99 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_config_gfxVarReceiver_h
#define mozilla_gfx_config_gfxVarReceiver_h

namespace mozilla {
namespace gfx {

class GfxVarUpdate;

// This allows downstream processes (such as PContent, PGPU) to listen for
// updates on gfxVarReceiver.
class gfxVarReceiver
{
public:
  virtual void OnVarChanged(const GfxVarUpdate& aVar) = 0;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_config_gfxVarReceiver_h
