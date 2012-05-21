/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=80 : 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_tabs_ContentThread_h
#define dom_tabs_ContentThread_h 1

#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/ipc/ScopedXREEmbed.h"
#include "ContentChild.h"

#undef _MOZ_LOG
#define _MOZ_LOG(s)  printf("[ContentProcess] %s", s)

namespace mozilla {
namespace dom {

/**
 * ContentProcess is a singleton on the content process which represents
 * the main thread where tab instances live.
 */
class ContentProcess : public mozilla::ipc::ProcessChild
{
    typedef mozilla::ipc::ProcessChild ProcessChild;

public:
    ContentProcess(ProcessHandle mParentHandle)
        : ProcessChild(mParentHandle)
    { }

    ~ContentProcess()
    { }

    NS_OVERRIDE
    virtual bool Init();
    NS_OVERRIDE
    virtual void CleanUp();

private:
    ContentChild mContent;
    mozilla::ipc::ScopedXREEmbed mXREEmbed;

    DISALLOW_EVIL_CONSTRUCTORS(ContentProcess);
};

}  // namespace dom
}  // namespace mozilla

#endif  // ifndef dom_tabs_ContentThread_h
