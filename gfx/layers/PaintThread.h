/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_PAINTTHREAD_H
#define MOZILLA_LAYERS_PAINTTHREAD_H

#include "base/platform_thread.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace gfx {
class DrawTarget;
class DrawTargetCapture;
};

namespace layers {

class PaintThread final
{
public:
  static void Start();
  static void Shutdown();
  static PaintThread* Get();
  void PaintContents(gfx::DrawTargetCapture* aCapture,
                     gfx::DrawTarget* aTarget);
  // Sync Runnables need threads to be ref counted,
  // But this thread lives through the whole process.
  // We're only temporarily using sync runnables so
  // Override release/addref but don't do anything.
  void Release();
  void AddRef();

private:
  bool IsOnPaintThread();
  bool Init();
  void ShutdownImpl();
  void InitOnPaintThread();

  static StaticAutoPtr<PaintThread> sSingleton;
  RefPtr<nsIThread> mThread;
  PlatformThreadId mThreadId;
};

} // namespace layers
} // namespace mozilla

#endif