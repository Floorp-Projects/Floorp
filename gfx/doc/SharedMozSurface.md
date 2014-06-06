Shared MozSurface {#mozsurface}
==========

**This document is work in progress.  Some information may be missing or incomplete.**

Shared MozSurfaces represent an important use case of MozSurface, anything that is in the MozSurface design document also applies to shared MozSurfaces.

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

Shared MozSurfaces represent an important use case of MozSurface, it has the same requirements as MozSurface.

## TextureClient and TextureHost

TextureClient and TextureHost are the closest abstractions we currently have to MozSurface.
Inline documentation about TextureClient and TextureHost can be found in:

* [gfx/layers/client/TextureClient.h](http://dxr.mozilla.org/mozilla-central/source/gfx/layers/client/TextureClient.h)
* [gfx/layers/composite/TextureHost.h](http://dxr.mozilla.org/mozilla-central/source/gfx/layers/composite/TextureHost.h)

TextureClient is the client-side handle on a MozSurface, while TextureHost is the equivalent host-side representation. There can only be one TextureClient for a given TextureHost, and one TextureHost for a given TextureClient. Likewise, there can only be one shared object for a given TextureClient/TextureHost pair.

A MozSurface containing data that is shared between a client process and a host process exists in the following form:

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

## Alternative solutions

### Sending ownership back and forth between the client and host sides through message passing, intead of sharing.

The current design of MozSurface makes the surface accessible from both sides at the same time, forcing us to do Locking and have a hand shake around deallocating the shared data, while using pure message passing and making the surface accessible only from one side at a time would avoid these complications.

Using pure message passing was actually the first approach we tried when we created the first version of TextureClient and TextureHost. This strategy failed in several places, partly because of some legacy in Gecko's architecture, and partly because of some of optimizations we do to avoid copying surfaces.

We need a given surface to be accessible on both the client and host for the following reasons:

* Gecko can at any time require read access on the client side to a surface that is shared with the host process, for example to build a temporary layer manager and generate a screenshot. This is mostly a legacy problem.
* We do some copy-on-write optimizations on surfaces that are shared with the compositor in order to keep invalid regions as small as possible. Out tiling implementation is an example of that.
* Our buffer rotation code on scrollable non-tiled layers also requires a synchronization on the client side between the front and back buffers, while the front buffer is used on the host side.

## Testing

TODO - How can we make shared MozSurfaces more testable and what should we test?

## Future work

### Rename TextureClient/TextureHost

The current terminology is very confusing.

### Unify TextureClient and TextureHost

TextureClient and TextureHost should live under a common interface to better hide the IPC details. The base classe should only expose the non-ipc related methods such as Locking, access through a DrawTarget, access to a TextureSource.

## Comparison with other APIs
