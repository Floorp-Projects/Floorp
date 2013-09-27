/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginBackgroundDestroyer.h"
#include "gfxSharedImageSurface.h"

using namespace mozilla;
using namespace plugins;

PluginBackgroundDestroyerParent::PluginBackgroundDestroyerParent(gfxASurface* aDyingBackground)
  : mDyingBackground(aDyingBackground)
{
}

PluginBackgroundDestroyerParent::~PluginBackgroundDestroyerParent()
{
}

void
PluginBackgroundDestroyerParent::ActorDestroy(ActorDestroyReason why)
{
    switch(why) {
    case Deletion:
    case AncestorDeletion:
        if (gfxSharedImageSurface::IsSharedImage(mDyingBackground)) {
            gfxSharedImageSurface* s =
                static_cast<gfxSharedImageSurface*>(mDyingBackground.get());
            DeallocShmem(s->GetShmem());
        }
        break;
    default:
        // We're shutting down or crashed, let automatic cleanup
        // take care of our shmem, if we have one.
        break;
    }
}
