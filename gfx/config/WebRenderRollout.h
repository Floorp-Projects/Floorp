/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_config_WebRenderRollout_h
#define mozilla_gfx_config_WebRenderRollout_h

#include "mozilla/Maybe.h"

namespace mozilla {
namespace gfx {

class WebRenderRollout final {
 public:
  static void Init();
  static Maybe<bool> CalculateQualifiedOverride();
  static bool CalculateQualified();

  WebRenderRollout() = delete;
  ~WebRenderRollout() = delete;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_config_WebRenderRollout_h
