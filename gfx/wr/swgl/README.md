# swgl

Software OpenGL implementation for WebRender

## Overview
This is a relatively simple single threaded software rasterizer designed
for use by WebRender. It will shade one quad at a time using a 4xf32 vector
with one vertex per lane. It rasterizes quads usings spans and shades that
span 4 pixels at a time.

## Building
clang-cl is required to build on Windows. This can be done by installing
the llvm binaries from https://releases.llvm.org/ and adding the installation
to the path with something like `set PATH=%PATH%;C:\Program Files\LLVM\bin`.
Then `set CC=clang-cl` and `set CXX=clang-cl`. That should be sufficient
for `cc-rs` to use `clang-cl` instead of `cl`.

## Extensions
SWGL contains a number of OpenGL and GLSL extensions designed to both ease
integration with WebRender and to help accelerate span rasterization.

GLSL extension intrinsics are generally prefixed with `swgl_` to distinguish
them from other items in the GLSL namespace.

Inside GLSL, the `SWGL` preprocessor token is defined so that usage of SWGL
extensions may be conditionally compiled.

```
void swgl_clipMask(sampler2D mask, vec2 offset, vec2 bb_origin, vec2 bb_size);
```

When called from the the vertex shader, this specifies a clip mask texture to
be used to mask the currently drawn primitive while blending is enabled. This
mask will only apply to the current primitive.

The mask must be an R8 texture that will be interpreted as alpha weighting
applied to the source pixel prior to the blend stage. It is sampled 1:1 with
nearest filtering without any applied transform. The given offset specifies
the positioning of the clip mask relative to the framebuffer's viewport.

The supplied bounding box constrains sampling of the clip mask to only fall
within the given rectangle, specified relative to the clip mask offset.
Anything falling outside this rectangle will be clipped entirely. If the
rectangle is empty, then the clip mask will be ignored.

