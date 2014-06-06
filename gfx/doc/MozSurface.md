MozSurface {#mozsurface}
==========

**This document is work in progress.  Some information may be missing or incomplete.**

## Goals

We need to be able to safely and efficiently render web content into surfaces that may be shared accross processes.
MozSurface is a cross-process and backend-independent Surface API and not a stream API.

## Owner

Nicolas Silva

## Definitions

## Use cases

Drawing web content into a surface and share it with the compositor process to display it on the screen without copies.

## Requirement

* It must be possible to efficiently share a MozSurface with a separate thread or process through IPDL
* It must be possible to obtain read access a MozSurface on both the client and the host side at the same time.
* The creation, update and destrution of surfaces must be safe and race-free. In particular, the ownership of the shared data must be clearly defined.
* MozSurface must be a cross-backend/cross-platform abstraction that we will use on all of the supported platforms.
* It must be possible to efficiently draw into a MozSurface using Moz2D.
* While it should be possible to share MozSurfaces accross processes, it should not be limited to that. MozSurface should also be the preferred abstraction for use with surfaces that are not shared with the compositor process.

## TextureClient and TextureHost

TextureClient and TextureHost are the closest abstractions we currently have to MozSurface. The current plan is to evolve TextureClient into MozSurface. In its current state, TextureClient doesn't meet all the requirements and desisgn decisions of MozSurface yet.

In particular, TextureClient/TextureHost are designed around cross-process sharing specifically. See the SharedMozSurface design document for more information about TextureClient and TextureHost.

## Locking semantics

In order to access the shared surface data users of MozSurface must acquire and release a lock on the surface, specifying the open mode (read/write/read+write).

    bool Lock(OpenMode aMode);
    void Unlock();

This locking API has two purposes:

* Ensure that access to the shared data is race-free.
* Let the implemetation do whatever is necessary for the user to have access to the data. For example it can be mapping and unmapping the surface data in memory if the underlying backend requires it.

The lock is expected to behave as a cross-process blocking read/write lock that is not reentrant.

## Immutable surfaces

In some cases we know in advance that a surface will not be modified after it has been shared. This is for example true for video frames. In this case the surface can be marked as immutable and the underlying implementation doesn't need to hold an actual blocking lock on the shared data.
Trying to acquire a write lock on a MozSurface that is marked as immutable and already shared must fail (return false).
Note that it is still required to use the Lock/Unlock API to read the data, in order for the implementation to be able to properly map and unmap the memory. This is just an optimization and a safety check.

## Drawing into a surface

In most cases we want to be able to paint directly into a surface through the Moz2D API.

A surface lets you *borrow* a DrawTarget that is only valid between Lock and Unlock.

    DrawTarget* GetAsDrawTarget();

It is invalid to hold a reference to the DrawTarget after Unlock, and a different DrawTarget may be obtained during the next Lock/Unlock interval.

In some cases we want to use MozSurface without drawing into it. For instance to share video frames accross processes. Some surface types may also not be accessible through a DrawTarget (for example YCbCr surfaces).

    bool CanExposeDrawTarget();

helps with making sure that a Surface supports exposing a Moz2D DrawTarget.

## Using a MozSurface as a source for Compositing

To interface with the Compositor API, MozSurface gives access to TextureSource objects. TextureSource is the cross-backend representation of a texture that Compositor understands.
While MozSurface handles memory management of (potentially shared) texture data, TextureSource is only an abstraction for Compositing.

## Fence synchronization

TODO: We need to figure this out. Right now we have a Gonk specific implementation, but no cross-platform abstraction/design.

## Ownership of the shared data

MozSurface (TextureClient/TextureHost in its current form) defines ownership rules that depend on the configuration of the surface, in order to satisy efficiency and safety requirements.

These rules rely on the fact that the underlying shared data is strictly owned by the MozSurface. This means that keeping direct references to the shared data is illegal and unsafe.

## Internal buffers / direct texturing

Some MozSurface implementations use CPU-side shared memory to share the texture data accross processes, and require a GPU texture upload when interfacing with a TextureSource. In this case we say that the surface has an internal buffer (because it is implicitly equivalent to double buffering where the shared data is the back buffer and the GPU side texture is the front buffer). We also say that it doesn't do "direct texturing" meaning that we don't draw directly into the GPU-side texture.

Examples:

 * Shmem MozSurface + OpenGL TextureSource: Has an internal buffer (no direct texturing)
 * Gralloc MozSurface + Gralloc TextureSource: No internal buffer (direct texturing)

While direct texturing is usually the most efficient way, it is not always available depending on the platform and the required allocation size or format. Textures with internal buffers have less restrictions around locking since the host side will only need to read from the MozSurface once per update, meaning that we can often get away with single buffering where we would need double buffering with direct texturing.

## Alternative solutions

## Backends

We have MozSurface implementaions (classes inheriting from TextureClient/TextureHost) for OpenGL, Software, D3D9, and D3D11 backends.
Some implemtations can be used with any backend (ex. ShmemTextureClient/Host).

## Users of MozSurface

MozSurface is the mechanism used by layers to share surfaces with the compositor, but it is not limited to layers. It should be used by anything that draws into a surface that may be shared with the compositor thread.

## Testing

TODO - How can we make MozSurface more testable and what should we test?

## Future work

### Using a MozSurface as a source for Drawing

MozSurface should be able to expose a borrowed Moz2D SourceSurface that is valid between Lock and Unlock similarly to how it exposes a DrawTarget.

## Comparison with other APIs

MozSurface is somewhat equivalent to Gralloc on Android/Gonk: it is a reference counted cross-process surface with locking semantics. While Gralloc can interface itself with OpenGL textures for compositing, MozSurface can interface itself to TextureSource objects.

MozSurface should not be confused with higher level APIs such as EGLStream. A swap-chain API like EGLStream can be implemented on top of MozSurface, but MozSurface's purpose is to define and manage the memory and resources of shared texture data.

