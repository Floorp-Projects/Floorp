/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginBackgroundDestroyer
#define dom_plugins_PluginBackgroundDestroyer

#include "mozilla/plugins/PPluginBackgroundDestroyerChild.h"
#include "mozilla/plugins/PPluginBackgroundDestroyerParent.h"

#include "gfxASurface.h"
#include "gfxSharedImageSurface.h"

namespace mozilla {
namespace plugins {

/**
 * When instances of this class are destroyed, the old background goes
 * along with them, completing the destruction process (whether or not
 * the plugin stayed alive long enough to ack).
 */
class PluginBackgroundDestroyerParent : public PPluginBackgroundDestroyerParent {
public:
    PluginBackgroundDestroyerParent(gfxASurface* aDyingBackground)
      : mDyingBackground(aDyingBackground)
    { }

    virtual ~PluginBackgroundDestroyerParent() { }

private:
    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
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

    nsRefPtr<gfxASurface> mDyingBackground;
};

/**
 * This class exists solely to instruct its instance to release its
 * current background, a new one may be coming.
 */
class PluginBackgroundDestroyerChild : public PPluginBackgroundDestroyerChild {
public:
    PluginBackgroundDestroyerChild() { }
    virtual ~PluginBackgroundDestroyerChild() { }

private:
    // Implementing this for good hygiene.
    NS_OVERRIDE
    virtual void ActorDestroy(ActorDestroyReason why)
    { }
};

} // namespace plugins
} // namespace mozilla

#endif  // dom_plugins_PluginBackgroundDestroyer
