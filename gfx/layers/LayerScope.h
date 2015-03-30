/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSCOPE_H
#define GFX_LAYERSCOPE_H

#include <stdint.h>
#include <mozilla/UniquePtr.h>

namespace mozilla {

namespace gl { class GLContext; }

namespace layers {

namespace layerscope { class Packet; }

struct EffectChain;
class LayerComposite;

class LayerScope {
public:
    static void SendEffectChain(gl::GLContext* aGLContext,
                                const EffectChain& aEffectChain,
                                int aWidth,
                                int aHeight);
    static void SendLayer(LayerComposite* aLayer,
                          int aWidth,
                          int aHeight);
    static void SendLayerDump(UniquePtr<layerscope::Packet> aPacket);
    static bool CheckSendable();
    static void CleanLayer();
    static void SetHWComposed();

private:
    static void Init();
};

// Perform BeginFrame and EndFrame automatically
class LayerScopeAutoFrame {
public:
    explicit LayerScopeAutoFrame(int64_t aFrameStamp);
    ~LayerScopeAutoFrame();

private:
    static void BeginFrame(int64_t aFrameStamp);
    static void EndFrame();
};

} /* layers */
} /* mozilla */

#endif /* GFX_LAYERSCOPE_H */
