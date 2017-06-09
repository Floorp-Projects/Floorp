/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_PAINTTHREAD_H
#define MOZILLA_LAYERS_PAINTTHREAD_H

#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

class PaintThread final
{
public:
  static void Start();
  static void Shutdown();
  static PaintThread* Get();

private:
  bool Init();
  void ShutdownImpl();
  static StaticAutoPtr<PaintThread> sSingleton;
  RefPtr<nsIThread> mThread;
};

} // namespace layers
} // namespace mozilla

#endif