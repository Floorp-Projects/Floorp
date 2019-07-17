/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dbg_Frame_inl_h
#define dbg_Frame_inl_h

#include "dbg/Frame.h"

#include "vm/GeneratorObject.h"

inline bool js::DebuggerFrame::hasGenerator() const {
  return !getReservedSlot(GENERATOR_INFO_SLOT).isUndefined();
}

inline js::DebuggerFrame::GeneratorInfo* js::DebuggerFrame::generatorInfo()
    const {
  MOZ_ASSERT(hasGenerator());
  return static_cast<GeneratorInfo*>(
      getReservedSlot(GENERATOR_INFO_SLOT).toPrivate());
}

#endif /* dbg_Frame_inl_h */
