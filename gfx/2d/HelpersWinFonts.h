/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

namespace mozilla {
namespace gfx {

extern BYTE sSystemTextQuality;

static BYTE GetSystemTextQuality() { return sSystemTextQuality; }

static AntialiasMode GetSystemDefaultAAMode() {
  AntialiasMode defaultMode = AntialiasMode::SUBPIXEL;

  switch (GetSystemTextQuality()) {
    case CLEARTYPE_QUALITY:
      defaultMode = AntialiasMode::SUBPIXEL;
      break;
    case ANTIALIASED_QUALITY:
      defaultMode = AntialiasMode::GRAY;
      break;
    case DEFAULT_QUALITY:
      defaultMode = AntialiasMode::NONE;
      break;
  }

  return defaultMode;
}

}  // namespace gfx
}  // namespace mozilla
