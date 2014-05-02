/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginSurfaceParent_h
#define dom_plugins_PluginSurfaceParent_h

#include "mozilla/plugins/PPluginSurfaceParent.h"
#include "nsAutoPtr.h"
#include "mozilla/plugins/PluginMessageUtils.h"

#ifndef XP_WIN
#error "This header is for Windows only."
#endif

class gfxASurface;

namespace mozilla {
namespace plugins {

class PluginSurfaceParent : public PPluginSurfaceParent
{
public:
  PluginSurfaceParent(const WindowsSharedMemoryHandle& handle,
                      const gfxIntSize& size,
                      const bool transparent);
  ~PluginSurfaceParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  gfxASurface* Surface() { return mSurface; }

private:
  nsRefPtr<gfxASurface> mSurface;
};

} // namespace plugins
} // namespace mozilla

#endif // dom_plugin_PluginSurfaceParent_h
