/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUChild.h"

namespace mozilla {
namespace gfx {

GPUChild::GPUChild()
{
  MOZ_COUNT_CTOR(GPUChild);
}

GPUChild::~GPUChild()
{
  MOZ_COUNT_DTOR(GPUChild);
}

} // namespace gfx
} // namespace mozilla
