/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GPUProcessImpl.h"
#include "mozilla/ipc/IOThreadChild.h"

namespace mozilla {
namespace gfx {

using namespace ipc;

GPUProcessImpl::GPUProcessImpl(ProcessId aParentPid)
 : ProcessChild(aParentPid)
{
}

GPUProcessImpl::~GPUProcessImpl()
{
}

bool
GPUProcessImpl::Init()
{
  return mGPU.Init(ParentPid(),
                   IOThreadChild::message_loop(),
                   IOThreadChild::channel());
}

void
GPUProcessImpl::CleanUp()
{
}

} // namespace gfx
} // namespace mozilla
