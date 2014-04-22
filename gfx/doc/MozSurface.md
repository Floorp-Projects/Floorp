MozSurface {#mozsurface}
==========

**This document is work in progress.  Some information may be missing or incomplete.**

## Goals

We need to be able to safely and efficiently render web content into surfaces that may be shared accross processes.
MozSurface is a cross-process and backend-independent Surface API and not a stream API.

## Owner

Nicolas Silva

## Definitions

* Client and Host: In Gecko's compositing architecture, the client process is the producer, while the host process is the consumer side, where compositing takes place.

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

TextureClient and TextureHost are the closest abstractions we currently have to MozSurface.
Inline documentation about TextureClient and TextureHost can be found in:

* [gfx/layers/client/TextureClient.h](http://dxr.mozilla.org/mozilla-central/source/gfx/layers/client/TextureClient.h)
* [gfx/layers/composite/TextureHost.h](http://dxr.mozilla.org/mozilla-central/source/gfx/layers/composite/TextureHost.h)

TextureClient is the client-side handle on a MozSurface, while TextureHost is the equivalent host-side representation. There can only be one TextureClient for a given TextureHost, and one TextureHost for a given TextureClient. Likewise, there can only be one shared object for a given TextureClient/TextureHost pair.

A MozSurface containing data that is shared between a client process and a host process exists in the foolowing form:

```
                                 .
            Client process       .      Host process
                                 .
     ________________      ______________      ______________
    |                |    |              |    |              |
    | TextureClient  +----+ <SharedData> +----+ TextureHost  |
    |________________|    |______________|    |______________|
                                 .
                                 .
                                 .
    Figure 1) A Surface as seen by the client and the host processes
```

The above figure is a logical representation, not a class diagram.
`<SharedData>` is a placeholder for whichever platform specific surface type we are sharing, for example a Gralloc buffer on Gonk or a D3D11 texture on Windows.

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

In some cases we want to use MozSurface without Drawing into it. For instance to share video frames accross processes. Some surface types may also not be accessible through a DrawTarget (for example YCbCr surfaces).

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

## Deallocation protocol

The shared data is accessible by both the client-side and the host-side of the MozSurface. A deallocation protocol must be defined to handle which side deallocates the data, and to ensure that it doesn't cause any race condition.
The client side, which contains the web content's logic, always "decides" when a surface is needed or not. So the life time of a MozSurface is driven by the reference count of it's client-side handle (TextureClient).
When a TextureClient's reference count reaches zero, a "Remove" message is sent in order to let the host side that the shared data is not accessible on the client side and that it si safe for it to be deleted. The host side responds with a "Delete" message.


```
           client side                .         host side
                                      .
    (A) Client: Send Remove     -.    .
                                  \   .
                                   \  .   ... can receive and send ...
                                    \
        Can receive                  `--> (B) Host: Receive Remove
        Can't send                         |
                                      .-- (C) Host: Send Delete
                                     /
                                    / .   ... can't receive nor send ...
                                   /  .
    (D) Client: Receive Delete <--'   .
                                      .
    Figure 2) MozSurface deallocation handshake
```

This handshake protocol is twofold:

* It defines where and when it is possible to deallocate the shared data without races
* It makes it impossible for asynchronous messages to race with the destruction of the MozSurface.

### Deallocating on the host side

In the common case, the shared data is deallocated asynchronously on the host side. In this case the deallocation takes place at the point (C) of figure 2.

### Deallocating on the client side

In some rare cases, for instance if the underlying implementation requires it, the shared data must be deallocated on the client side. In such cases, deallocation happens at the point (D) of figure 2.

In some exceptional cases, this needs to happen synchronously, meaning that the client-side thread will block until the Delete message is received. This is supported but it is terrible for performance, so it should be avoided as much as possible.
Currently this is needed when shutting down a hardware-decoded video stream with libstagefright on Gonk, because the libstagefright unfortunately assumes it has full ownership over the shared data (gralloc buffers) and crashes if there are still users of the buffers.

### Sharing state

The above deallocation protocol of a MozSurface applies to the common case that is when the surface is shared between two processes. A Surface can also be deallocated while it is not shared.

The sharing state of a MozSurface can be one of the following:

* (1) Uninitialized (it doesn't have any shared data)
* (2) Local (it isn't shared with the another thread/process)
* (3) Shared (the state you would expect it to be most of the time)
* (4) Invalid (when for some rare cases we needed to force the deallocation of the shared data before the destruction of the TextureClient object).

Surfaces can move from state N to state N+1 and be deallocated in any of these states. It could be possible to move from Shared to Local, but we currently don't have a use case for it.

The deallocation protocol above, applies to the Shared state (3).
In the other cases:

* (1) Unitilialized: There is nothing to do.
* (2) Local: The shared data is deallocated by the client side without need for a handshake, since it is not shared with other threads.
* (4) Invalid: There is nothing to do (deallocation has already happenned).

## Internal buffers / direct texturing

Some MozSurface implementations use CPU-side shared memory to share the texture data accross processes, and require a GPU texture upload when interfacing with a TextureSource. In this case we say that the surface has an internal buffer (because it is implicitly equivalent to double buffering where the shared data is the back buffer and the GPU side texture is the front buffer). We also say that it doesn't do "direct texturing" meaning that we don't draw directly into the GPU-side texture.

Examples:

 * Shmem MozSurface + OpenGL TextureSource: Has an internal buffer (no direct texturing)
 * Gralloc MozSurface + Gralloc TextureSource: No internal buffer (direct texturing)

While direct texturing is usually the most efficient way, it is not always available depending on the platform and the required allocation size or format. Textures with internal buffers have less restrictions around locking since the host side will only need to read from the MozSurface once per update, meaning that we can often get away with single buffering where we would need double buffering with direct texturing.

## Alternative solutions

### Sending ownership back and forth between the client and host sides through message passing, intead of sharing.

The current design of MozSurface makes the surface accessible from both sides at the same time, forcing us to do Locking and have a hand shake around deallocating the shared data, while using pure message passing and making the surface accessible only from one side at a time would avoid these complications.

Using pure message passing was actually the first approach we tried when we created the first version of TextureClient and TextureHost. This strategy failed in several places, partly because of some legacy in Gecko's architecture, and partly because of some of optimizations we do to avoid copying surfaces.

We need a given surface to be accessible on both the client and host for the following reasons:

* Gecko can at any time require read access on the client side to a surface that is shared with the host process, for example to build a temporary layer manager and generate a screenshot. This is mostly a legacy problem.
* We do some copy-on-write optimizations on surfaces that are shared with the compositor in order to keep invalid regions as small as possible. Out tiling implementation is an example of that.
* Our buffer rotation code on scrollable non-tiled layers also requires a synchronization on the client side between the front and back buffers, while the front buffer is used on the host side.

## Backends

We have MozSurface implementaions (classes inheriting from TextureClient/TextureHost) for OpenGL, Software, D3D9, and D3D11 backends.
Some implemtations can be used with any backend (ex. ShmemTextureClient/Host).

## Users of MozSurface

MozSurface is the mechanism used by layers to share surfaces with the compositor, but it is not limited to layers. It should be used by anything that draws into a surface that may be shared with the compositor thread.

## Testing

TODO - How can we make MozSurface more testable and what should we test?

## Future work

### Rename TextureClient/TextureHost

The current terminology is very confusing.

### Unify TextureClient and TextureHost

TextureClient and TextureHost should live under a common interface to better hide the IPC details. The base classe should only expose the non-ipc related methods such as Locking, access through a DrawTarget, access to a TextureSource.

### Using a MozSurface as a source for Drawing

MozSurface should be able to expose a borrowed Moz2D SourceSurface that is valid between Lock and Unlock similarly to how it exposes a DrawTarget.

## Comparison with other APIs

MozSurface is somewhat equivalent to Gralloc on Android/Gonk: it is a reference counted cross-process surface with locking semantics. While Gralloc can interface itself with OpenGL textures for compositing, MozSurface can interface itself to TextureSource objects.

MozSurface should not be confused with higher level APIs such as EGLStream. A swap-chain API like EGLStream can be implemented on top of MozSurface, but MozSurface's purpose is to define and manage the memory and resources of shared texture data.

