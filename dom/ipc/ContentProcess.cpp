/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=80 : 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IOThreadChild.h"

#include "ContentProcess.h"

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace dom {

void
ContentProcess::SetAppDir(const nsACString& aPath)
{
  mXREEmbed.SetAppDir(aPath);
}

bool
ContentProcess::Init()
{
    mContent.Init(IOThreadChild::message_loop(),
                         ParentHandle(),
                         IOThreadChild::channel());
    mXREEmbed.Start();
    mContent.InitXPCOM();
    
    return true;
}

void
ContentProcess::CleanUp()
{
    mXREEmbed.Stop();
}

} // namespace tabs
} // namespace mozilla
