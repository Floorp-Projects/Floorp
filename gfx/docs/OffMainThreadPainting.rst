Off Main Thread Painting
========================

OMTP, or ‘off main thread painting’, is the component of Gecko that
allows us to perform painting of web content off of the main thread.
This gives us more time on the main thread for javascript, layout,
display list building, and other tasks which allows us to increase our
responsiveness.

Take a look at this `blog
post <https://mozillagfx.wordpress.com/2017/12/05/off-main-thread-painting/>`__
for an introduction.

Background
----------

Painting (or rasterization) is the last operation that happens in a
layer transaction before we forward it to the compositor. At this point,
all display items have been assigned to a layer and invalid regions have
been calculated and assigned to each layer.

The painted layer uses a content client to acquire a buffer for
painting. The main purpose of the content client is to allow us to
retain already painted content when we are scrolling a layer. We have
two main strategies for this, rotated buffer and tiling.

This is implemented with two class hierarchies. ``ContentClient`` for
rotated buffer and ``TiledContentClient`` for tiling. Additionally we
have two different painted layer implementations, ``ClientPaintedLayer``
and ``ClientTiledPaintedLayer``.

The main distinction between rotated buffer and tiling is the amount of
graphics surfaces required. Rotated buffer utilizes just a single buffer
for a frame but potentially requires painting into it multiple times.
Tiling uses multiple buffers but doesn’t require painting into the
buffers multiple times.

Once the painted layer has a surface (or surfaces with tiling) to paint
into, they are wrapped in a ``DrawTarget`` of some form and a callback
to ``FrameLayerBuilder`` is called. This callback uses the assigned
display items and invalid regions to trigger rasterization. Each
``nsDisplayItem`` has their ``Paint`` method called with the provided
``DrawTarget`` that represents the surface, and they paint into it.

High level
----------

The key abstraction that allows us to paint off of the main thread is
``DrawTargetCapture`` [1]_. ``DrawTargetCapture`` is a special
``DrawTarget`` which records all draw commands for replaying to another
draw target in the local process. This is similar to
``DrawTargetRecording``, but only holds a reference to resources instead
of copying them into the command stream. This allows the command stream
to be much more lightweight than ``DrawTargetRecording``.

OMTP works by instrumenting the content clients to use a capture target
for all painting [2]_ [3]_ [4]_ [5]_. This capture draw target records all
the operations that would normally be performed directly on the
surface’s draw target. Once we have all of the commands, we send the
capture and surface draw target to the ``PaintThread`` [6]_ where the
commands are replayed onto the surface. Once the rasterization is done,
we forward the layer transaction to the compositor.

Tiling and parallel painting
----------------------------

We can make one additional improvement if we are using tiling as our
content client backend.

When we are tiling, the screen is subdivided into a grid of equally
sized surfaces and draw commands are performed on the tiles they affect.
Each tile is independent of the others, so we’re able to parallelize
painting by using a worker thread pool and dispatching a task for each
tile individually.

This is commonly referred to as P-OMTP or parallel painting.

Main thread rasterization
-------------------------

Even with OMTP it’s still possible for the main thread to perform
rasterization. A common pattern for painting code is to create a
temporary draw target, perform drawing with it, take a snapshot, and
then draw the snapshot onto the main draw target. This is done for
blurs, box shadows, text shadows, and with the basic layer manager
fallback.

If the temporary draw target is not a draw target capture, then this
will perform rasterization on the main thread. This can be bad as it
lowers our parallelism and can cause contention with content backends,
like Direct2D, that use locking around shared resources.

To work around this, we changed the main thread painting code to use a
draw target capture for these operations and added a source surface
capture [7]_ which only resolves the painting of the draw commands when
needed on the paint thread.

There are still possible cases we can perform main thread rasterization,
but we try and address them when they come up.

Out of memory issues
--------------------

The web is very complex, and so we can sometimes have a large amount of
draw commands for a content paint. We’ve observed OOM errors for capture
command lists that have grown to be 200MiB large.

We initially tried to mitigate this by lowering the overhead of capture
command lists. We do this by filtering commands that don’t actually
change the draw target state and folding consecutive transform changes,
but that was not always enough. So we added the ability for our draw
target capture’s to flush their command lists to the surface draw target
while we are capturing on the main thread [8]_.

This is triggered by a configurable memory limit. Because this
introduces a new source of main thread rasterization we try to balance
setting this too low and suffering poor performance, or setting this too
high and suffering crashes.

Synchronization
---------------

OMTP is conceptually simple, but in practice it relies on subtle code to
ensure thread safety. This was the most arguably the most difficult part
of the project.

There are roughly four areas that are critical.

1. Compositor message ordering

   Immediately after we queue the async paints to be asynchronously
   completed, we have a problem. We need to forward the layer
   transaction at some point, but the compositor cannot process the
   transaction until all async paints have finished. If it did, it could
   access unfinished painted content.

   We obviously can’t block on the async paints completing as that would
   beat the whole point of OMTP. We also can’t hold off on sending the
   layer transaction to ``IPDL``, as we’d trigger race conditions for
   messages sent after the layer transaction is built but before it is
   forwarded. Reftest and other code assumes that messages sent after a
   layer transaction to the compositor are processed after that layer
   transaction is processed.

   The solution is to forward the layer transaction to the compositor
   over ``IPDL``, but flag the message channel to start postponing
   messages [9]_. Then once all async paints have completed, we unflag
   the message channel and all postponed messages are sent [10]_. This
   allows us to keep our message ordering guarantees and not have to
   worry about scheduling a runnable in the future.

2. Texture clients

   The backing store for content surfaces is managed by texture client.
   While async paints are executing, it’s possible for shutdown or any
   number of things to happen that could cause layer manager, all
   layers, all content clients, and therefore all texture clients to be
   destroyed. Therefore it’s important that we keep these texture
   clients alive throughout async painting. Texture clients also manage
   IPC resources and must be destroyed on the main thread, so we are
   careful to do that [11]_.

3. Double buffering

   We currently double buffer our content painting - our content clients
   only ever have zero or one texture that is available to be painted
   into at any moment.

   This implies that we cannot start async painting a layer tree while
   previous async paints are still active as this would lead to awful
   races. We also don’t support multiple nested sets of postponed IPC
   messages to allow sending the first layer transaction to the
   compositor, but not the second.

   To prevent issues with this, we flush all active async paints before
   we begin to paint a new layer transaction [12]_.

   There was some initial debate about implementing triple buffering for
   content painting, but we have not seen evidence it would help us
   significantly.

4. Moz2D thread safety

   Finally, most Moz2D objects were not thread safe. We had to insert
   special locking into draw target and source surface as they have a
   special copy on write relationship that must be consistent even if
   they are on different threads.

   Some platform specific resources like fonts needed locking added in
   order to be thread safe. We also did some work to make filter nodes
   work with multiple threads executing them at the same time.

Browser process
---------------

Currently only content processes are able to use OMTP.

This restriction was added because of concern about message ordering
between ``APZ`` and OMTP. It might be able to lifted in the future.

Important bugs
--------------

1. `OMTP Meta <https://bugzilla.mozilla.org/show_bug.cgi?id=omtp>`__
2. `Enable on
   Windows <https://bugzilla.mozilla.org/show_bug.cgi?id=1403935>`__
3. `Enable on
   OSX <https://bugzilla.mozilla.org/show_bug.cgi?id=1422392>`__
4. `Enable on
   Linux <https://bugzilla.mozilla.org/show_bug.cgi?id=1432531>`__
5. `Parallel
   painting <https://bugzilla.mozilla.org/show_bug.cgi?id=1425056>`__

Code links
----------

.. [1]  `DrawTargetCapture <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/2d/DrawTargetCapture.h#22>`__
.. [2]  `Creating DrawTargetCapture for rotated
    buffer <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/layers/client/ContentClient.cpp#185>`__
.. [3]  `Dispatch DrawTargetCapture for rotated
    buffer <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/layers/client/ClientPaintedLayer.cpp#99>`__
.. [4]  `Creating DrawTargetCapture for
    tiling <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/layers/client/TiledContentClient.cpp#714>`__
.. [5]  `Dispatch DrawTargetCapture for
    tiling <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/layers/client/MultiTiledContentClient.cpp#288>`__
.. [6]  `PaintThread <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/layers/PaintThread.h#53>`__
.. [7]  `SourceSurfaceCapture <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/2d/SourceSurfaceCapture.h#19>`__
.. [8] `Sync flushing draw
    commands <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/2d/DrawTargetCapture.h#165>`__
.. [9]  `Postponing messages for
    PCompositorBridge <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/layers/ipc/CompositorBridgeChild.cpp#1319>`__
.. [10]  `Releasing messages for
    PCompositorBridge <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/layers/ipc/CompositorBridgeChild.cpp#1303>`__
.. [11] `Releasing texture clients on main
    thread <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/layers/ipc/CompositorBridgeChild.cpp#1170>`__
.. [12] `Flushing async
    paints <https://searchfox.org/mozilla-central/rev/dd965445ec47fbf3cee566eff93b301666bda0e1/gfx/layers/client/ClientLayerManager.cpp#289>`__
