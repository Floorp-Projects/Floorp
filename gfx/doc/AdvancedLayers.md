Advanced Layers
==============

Advanced Layers is a new method of compositing layers in Gecko. This document serves as a technical
overview and provides a short walk-through of its source code.

Overview
-------------

Advanced Layers attempts to group as many GPU operations as it can into a single draw call. This is
a common technique in GPU-based rendering called "batching". It is not always trivial, as a
batching algorithm can easily waste precious CPU resources trying to build optimal draw calls.

Advanced Layers reuses the existing Gecko layers system as much as possible. Huge layer trees do
not currently scale well (see the future work section), so opportunities for batching are currently
limited without expending unnecessary resources elsewhere. However, Advanced Layers has a few
benefits:

 * It submits smaller GPU workloads and buffer uploads than the existing compositor.
 * It needs only a single pass over the layer tree.
 * It uses occlusion information more intelligently.
 * It is easier to add new specialized rendering paths and new layer types.
 * It separates compositing logic from device logic, unlike the existing compositor.
 * It is much faster at rendering 3d scenes or complex layer trees.
 * It has experimental code to use the z-buffer for occlusion culling.

Because of these benefits we hope that it provides a significant improvement over the existing
compositor.

Advanced Layers uses the acronym "MLG" and "MLGPU" in many places. This stands for "Mid-Level
Graphics", the idea being that it is optimized for Direct3D 11-style rendering systems as opposed
to Direct3D 12 or Vulkan.

LayerManagerMLGPU
------------------------------

Advanced layers does not change client-side rendering at all. Content still uses Direct2D (when
possible), and creates identical layer trees as it would with a normal Direct3D 11 compositor. In
fact, Advanced Layers re-uses all of the existing texture handling and video infrastructure as
well, replacing only the composite-side layer types.

Advanced Layers does not create a `LayerManagerComposite` - instead, it creates a
`LayerManagerMLGPU`. This layer manager does not have a `Compositor` - instead, it has an
`MLGDevice`, which roughly abstracts the Direct3D 11 API. (The hope is that this API is easily
interchangeable for something else when cross-platform or software support is needed.)

`LayerManagerMLGPU` also dispenses with the old "composite" layers for new layer types. For
example, `ColorLayerComposite` becomes `ColorLayerMLGPU`. Since these layer types implement
`HostLayer`, they integrate with `LayerTransactionParent` as normal composite layers would.

Rendering Overview
----------------------------

The steps for rendering are described in more detail below, but roughly the process is:

1. Sort layers front-to-back.
2. Create a dependency tree of render targets (called "views").
3. Accumulate draw calls for all layers in each view.
4. Upload draw call buffers to the GPU.
5. Execute draw commands for each view.

Advanced Layers divides the layer tree into "views" (`RenderViewMLGPU`), which correspond to a
render target. The root layer is represented by a view corresponding to the screen. Layers that
require intermediate surfaces have temporary views. Layers are analyzed front-to-back, and rendered
back-to-front within a view. Views themselves are rendered front-to-back, to minimize render target
switching.

Each view contains one or more rendering passes (`RenderPassMLGPU`). A pass represents a single
draw command with one or more rendering items attached to it. For example, a `SolidColorPass` item
contains a rectangle and an RGBA value, and many of these can be drawn with a single GPU call.

When considering a layer, views will first try to find an existing rendering batch that can support
it. If so, that pass will accumulate another draw item for the layer. Otherwise, a new pass will be
added.

When trying to find a matching pass for a layer, there is a tradeoff in CPU time versus the GPU
time saved by not issuing another draw commands. We generally care more about CPU time, so we do
not try too hard in matching items to an existing batch.

After all layers have been processed, there is a "prepare" step. This copies all accumulated draw
data and uploads it into vertex and constant buffers in the GPU.

Finally, we execute rendering commands. At the end of the frame, all batches and (most) constant
buffers are thrown away.

Shaders Overview
-------------------------------------

Advanced Layers currently has five layer-related shader pipelines:

 - Textured (PaintedLayer, ImageLayer, CanvasLayer)
 - ComponentAlpha (PaintedLayer with component-alpha)
 - YCbCr (ImageLayer with YCbCr video)
 - Color (ColorLayers)
 - Blend (ContainerLayers with mix-blend modes)

There are also three special shader pipelines:

 - MaskCombiner, which is used to combine mask layers into a single texture.
 - Clear, which is used for fast region-based clears when not directly supported by the GPU.
 - Diagnostic, which is used to display the diagnostic overlay texture.

The layer shaders follow a unified structure. Each pipeline has a vertex and pixel shader.
The vertex shader takes a layers ID, a z-buffer depth, a unit position in either a unit
square or unit triangle, and either rectangular or triangular geometry. Shaders can also
have ancillary data needed like texture coordinates or colors.

Most of the time, layers have simple rectangular clips with simple rectilinear transforms, and
pixel shaders do not need to perform masking or clipping. For these layers we use a fast-path
pipeline, using unit-quad shaders that are able to clip geometry so the pixel shader does not
have to. This type of pipeline does not support complex masks.

If a layer has a complex mask, a rotation or 3d transform, or a complex operation like blending,
then we use shaders capable of handling arbitrary geometry. Their input is a unit triangle,
and these shaders are generally more expensive.

All of the shader-specific data is modelled in ShaderDefinitionsMLGPU.h.

CPU Occlusion Culling
-------------------------------------

By default, Advanced Layers performs occlusion culling on the CPU. Since layers are visited
front-to-back, this is simply a matter of accumulating the visible region of opaque layers, and
subtracting it from the visible region of subsequent layers. There is a major difference
between this occlusion culling and PostProcessLayers of the old compositor: AL performs culling
after invalidation, not before. Completely valid layers will have an empty visible region.

Most layer types (with the exception of images) will intelligently split their draw calls into a
batch of individual rectangles, based on their visible region.

Z-Buffering and Occlusion
-------------------------------------

Advanced Layers also supports occlusion culling on the GPU, using a z-buffer. This is disabled by
default currently since it is significantly costly on integrated GPUs. When using the z-buffer, we
separate opaque layers into a separate list of passes. The render process then uses the following
steps:

 1. The depth buffer is set to read-write.
 2. Opaque batches are executed.,
 3. The depth buffer is set to read-only.
 4. Transparent batches are executed.

The problem we have observed is that the depth buffer increases writes to the GPU, and on
integrated GPUs this is expensive - we have seen draw call times increase by 20-30%, which is the
wrong direction we want to take on battery life. In particular on a full screen video, the call to
ClearDepthStencilView plus the actual depth buffer write of the video can double GPU time.

For now the depth-buffer is disabled until we can find a compelling case for it on non-integrated
hardware.

Clipping
-------------------------------------

Clipping is a bit tricky in Advanced Layers. We cannot use the hardware "scissor" feature, since the
clip can change from instance to instance within a batch. And if using the depth buffer, we
cannot write transparent pixels for the clipped area. As a result we always clip opaque draw rects
in the vertex shader (and sometimes even on the CPU, as is needed for sane texture coordiantes).
Only transparent items are clipped in the pixel shader. As a result, masked layers and layers with
non-rectangular transforms are always considered transparent, and use a more flexible clipping
pipeline.

Plane Splitting
---------------------

Plane splitting is when a 3D transform causes a layer to be split - for example, one transparent
layer may intersect another on a separate plane. When this happens, Gecko sorts layers using a BSP
tree and produces a list of triangles instead of draw rects.

These layers cannot use the "unit quad" shaders that support the fast clipping pipeline. Instead
they always use the full triangle-list shaders that support extended vertices and clipping.

This is the slowest path we can take when building a draw call, since we must interact with the
polygon clipping and texturing code.

Masks
---------

For each layer with a mask attached, Advanced Layers builds a `MaskOperation`. These operations
must resolve to a single mask texture, as well as a rectangular area to which the mask applies. All
batched pixel shaders will automatically clip pixels to the mask if a mask texture is bound. (Note
that we must use separate batches if the mask texture changes.)

Some layers have multiple mask textures. In this case, the MaskOperation will store the list of
masks, and right before rendering, it will invoke a shader to combine these masks into a single texture.

MaskOperations are shared across layers when possible, but are not cached across frames.

BigImage Support
--------------------------

ImageLayers and CanvasLayers can be tiled with many individual textures. This happens in rare cases
where the underlying buffer is too big for the GPU. Early on this caused problems for Advanced
Layers, since AL required one texture per layer. We implemented BigImage support by creating
temporary ImageLayers for each visible tile, and throwing those layers away at the end of the
frame.

Advanced Layers no longer has a 1:1 layer:texture restriction, but we retain the temporary layer
solution anyway. It is not much code and it means we do not have to split `TexturedLayerMLGPU`
methods into iterated and non-iterated versions.

Texture Locking
----------------------

Advanced Layers has a different texture locking scheme than the existing compositor. If a texture
needs to be locked, then it is locked by the MLGDevice automatically when bound to the current
pipeline. The MLGDevice keeps a set of the locked textures to avoid double-locking. At the end of
the frame, any textures in the locked set are unlocked.

We cannot easily replicate the locking scheme in the old compositor, since the duration of using
the texture is not scoped to when we visit the layer.

Buffer Measurements
-------------------------------

Advanced Layers uses constant buffers to send layer information and extended instance data to the
GPU. We do this by pre-allocating large constant buffers and mapping them with `MAP_DISCARD` at the
beginning of the frame. Batches may allocate into this up to the maximum bindable constant buffer
size of the device (currently, 64KB).

There are some downsides to this approach. Constant buffers are difficult to work with - they have
specific alignment requirements, and care must be taken not too run over the maximum number of
constants in a buffer. Another approach would be to store constants in a 2D texture and use vertex
shader texture fetches. Advanced Layers implemented this and benchmarked it to decide which
approach to use. Textures seemed to skew better on GPU performance, but worse on CPU, but this
varied depending on the GPU. Overall constant buffers performed best and most consistently, so we
have kept them.

Additionally, we tested different ways of performing buffer uploads. Buffer creation itself is
costly, especially on integrated GPUs, and especially so for immutable, immediate-upload buffers.
As a result we aggressively cache buffer objects and always allocate them as MAP_DISCARD unless
they are write-once and long-lived.

Buffer Types
------------

Advanced Layers has a few different classes to help build and upload buffers to the GPU. They are:

 - `MLGBuffer`. This is the low-level shader resource that `MLGDevice` exposes. It is the building
   block for buffer helper classes, but it can also be used to make one-off, immutable,
   immediate-upload buffers. MLGBuffers, being a GPU resource, are reference counted.
 - `SharedBufferMLGPU`. These are large, pre-allocated buffers that are read-only on the GPU and
   write-only on the CPU. They usually exceed the maximum bindable buffer size. There are three
   shared buffers created by default and they are automatically unmapped as needed: one for vertices,
   one for vertex shader constants, and one for pixel shader constants. When callers allocate into a
   shared buffer they get back a mapped pointer, a GPU resource, and an offset. When the underlying
   device supports offsetable buffers (like `ID3D11DeviceContext1` does), this results in better GPU
   utilization, as there are less resources and fewer upload commands.
 - `ConstantBufferSection` and `VertexBufferSection`. These are "views" into a `SharedBufferMLGPU`.
   They contain the underlying `MLGBuffer`, and when offsetting is supported, the offset
   information necessary for resource binding. Sections are not reference counted.
 - `StagingBuffer`. A dynamically sized CPU buffer where items can be appended in a free-form
   manner. The stride of a single "item" is computed by the first item written, and successive
   items must have the same stride. The buffer must be uploaded to the GPU manually. Staging buffers
   are appropriate for creating general constant or vertex buffer data. They can also write items in
   reverse, which is how we render back-to-front when layers are visited front-to-back. They can be
   uploaded to a `SharedBufferMLGPU` or an immutabler `MLGBuffer` very easily. Staging buffers are not
   reference counted.

Unsupported Features
--------------------------------

Currently, these features of the old compositor are not yet implemented.

 - OpenGL and software support (currently AL only works on D3D11).
 - APZ displayport overlay.
 - Diagnostic/developer overlays other than the FPS/timing overlay.
 - DEAA. It was never ported to the D3D11 compositor, but we would like it.
 - Component alpha when used inside an opaque intermediate surface.
 - Effects prefs. Possibly not needed post-B2G removal.
 - Widget overlays and underlays used by macOS and Android.
 - DefaultClearColor. This is Android specific, but is easy to added when needed.
 - Frame uniformity info in the profiler. Possibly not needed post-B2G removal.
 - LayerScope. There are no plans to make this work.

Future Work
--------------------------------

 - Refactor for D3D12/Vulkan support (namely, split MLGDevice into something less stateful and something else more low-level).
 - Remove "MLG" moniker and namespace everything.
 - Other backends (D3D12/Vulkan, OpenGL, Software)
 - Delete CompositorD3D11
 - Add DEAA support
 - Re-enable the depth buffer by default for fast GPUs
 - Re-enable right-sizing of inaccurately sized containers
 - Drop constant buffers for ancillary vertex data
 - Fast shader paths for simple video/painted layer cases

History
----------

Advanced Layers has gone through four major design iterations. The initial version used tiling -
each render view divided the screen into 128x128 tiles, and layers were assigned to tiles based on
their screen-space draw area. This approach proved not to scale well to 3d transforms, and so
tiling was eliminated.

We replaced it with a simple system of accumulating draw regions to each batch, thus ensuring that
items could be assigned to batches while maintaining correct z-ordering. This second iteration also
coincided with plane-splitting support.

On large layer trees, accumulating the affected regions of batches proved to be quite expensive.
This led to a third iteration, using depth buffers and separate opaque and transparent batch lists
to achieve z-ordering and occlusion culling.

Finally, depth buffers proved to be too expensive, and we introduced a simple CPU-based occlusion
culling pass. This iteration coincided with using more precise draw rects and splitting pipelines
into unit-quad, cpu-clipped and triangle-list, gpu-clipped variants.

