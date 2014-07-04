/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSCOPE_H
#define GFX_LAYERSCOPE_H

#include <stdint.h>

struct nsIntSize;

namespace mozilla {

namespace gl { class GLContext; }

namespace layers {

struct EffectChain;
class LayerComposite;

class LayerScope {
public:
    static void Init();
    static void DeInit();
    static void SendEffectChain(gl::GLContext* aGLContext,
                                const EffectChain& aEffectChain,
                                int aWidth,
                                int aHeight);
    static void SendLayer(LayerComposite* aLayer,
                          int aWidth,
                          int aHeight);
    static bool CheckSendable();
    static void CleanLayer();
};

// Perform BeginFrame and EndFrame automatically
class LayerScopeAutoFrame {
public:
    LayerScopeAutoFrame(int64_t aFrameStamp);
    ~LayerScopeAutoFrame();

private:
    static void BeginFrame(int64_t aFrameStamp);
    static void EndFrame();
};

} /* layers */
} /* mozilla */

#endif /* GFX_LAYERSCOPE_H */
