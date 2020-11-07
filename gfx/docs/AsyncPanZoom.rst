.. _apz:

Asynchronous Panning and Zooming
================================

**This document is a work in progress. Some information may be missing
or incomplete.**

.. image:: AsyncPanZoomArchitecture.png

Goals
-----

We need to be able to provide a visual response to user input with
minimal latency. In particular, on devices with touch input, content
must track the finger exactly while panning, or the user experience is
very poor. According to the UX team, 120ms is an acceptable latency
between user input and response.

Context and surrounding architecture
------------------------------------

The fundamental problem we are trying to solve with the Asynchronous
Panning and Zooming (APZ) code is that of responsiveness. By default,
web browsers operate in a “game loop” that looks like this:

::

       while true:
           process input
           do computations
           repaint content
           display repainted content

In browsers the “do computation” step can be arbitrarily expensive
because it can involve running event handlers in web content. Therefore,
there can be an arbitrary delay between the input being received and the
on-screen display getting updated.

Responsiveness is always good, and with touch-based interaction it is
even more important than with mouse or keyboard input. In order to
ensure responsiveness, we split the “game loop” model of the browser
into a multithreaded variant which looks something like this:

::

       Thread 1 (compositor thread)
       while true:
           receive input
           send a copy of input to thread 2
           adjust painted content based on input
           display adjusted painted content
       
       Thread 2 (main thread)
       while true:
           receive input from thread 1
           do computations
           repaint content
           update the copy of painted content in thread 1

This multithreaded model is called off-main-thread compositing (OMTC),
because the compositing (where the content is displayed on-screen)
happens on a separate thread from the main thread. Note that this is a
very very simplified model, but in this model the “adjust painted
content based on input” is the primary function of the APZ code.

The “painted content” is stored on a set of “layers”, that are
conceptually double-buffered. That is, when the main thread does its
repaint, it paints into one set of layers (the “client” layers). The
update that is sent to the compositor thread copies all the changes from
the client layers into another set of layers that the compositor holds.
These layers are called the “shadow” layers or the “compositor” layers.
The compositor in theory can continuously composite these shadow layers
to the screen while the main thread is busy doing other things and
painting a new set of client layers.

The APZ code takes the input events that are coming in from the hardware
and uses them to figure out what the user is trying to do (e.g. pan the
page, zoom in). It then expresses this user intention in the form of
translation and/or scale transformation matrices. These transformation
matrices are applied to the shadow layers at composite time, so that
what the user sees on-screen reflects what they are trying to do as
closely as possible.

Technical overview
------------------

As per the heavily simplified model described above, the fundamental
purpose of the APZ code is to take input events and produce
transformation matrices. This section attempts to break that down and
identify the different problems that make this task non-trivial.

Checkerboarding
~~~~~~~~~~~~~~~

The content area that is painted and stored in a shadow layer is called
the “displayport”. The APZ code is responsible for determining how large
the displayport should be. On the one hand, we want the displayport to
be as large as possible. At the very least it needs to be larger than
what is visible on-screen, because otherwise, as soon as the user pans,
there will be some unpainted area of the page exposed. However, we
cannot always set the displayport to be the entire page, because the
page can be arbitrarily long and this would require an unbounded amount
of memory to store. Therefore, a good displayport size is one that is
larger than the visible area but not so large that it is a huge drain on
memory. Because the displayport is usually smaller than the whole page,
it is always possible for the user to scroll so fast that they end up in
an area of the page outside the displayport. When this happens, they see
unpainted content; this is referred to as “checkerboarding”, and we try
to avoid it where possible.

There are many possible ways to determine what the displayport should be
in order to balance the tradeoffs involved (i.e. having one that is too
big is bad for memory usage, and having one that is too small results in
excessive checkerboarding). Ideally, the displayport should cover
exactly the area that we know the user will make visible. Although we
cannot know this for sure, we can use heuristics based on current
panning velocity and direction to ensure a reasonably-chosen displayport
area. This calculation is done in the APZ code, and a new desired
displayport is frequently sent to the main thread as the user is panning
around.

Multiple layers
~~~~~~~~~~~~~~~

Consider, for example, a scrollable page that contains an iframe which
itself is scrollable. The iframe can be scrolled independently of the
top-level page, and we would like both the page and the iframe to scroll
responsively. This means that we want independent asynchronous panning
for both the top-level page and the iframe. In addition to iframes,
elements that have the overflow:scroll CSS property set are also
scrollable, and also end up on separate scrollable layers. In the
general case, the layers are arranged in a tree structure, and so within
the APZ code we have a matching tree of AsyncPanZoomController (APZC)
objects, one for each scrollable layer. To manage this tree of APZC
instances, we have a single APZCTreeManager object. Each APZC is
relatively independent and handles the scrolling for its associated
layer, but there are some cases in which they need to interact; these
cases are described in the sections below.

Hit detection
~~~~~~~~~~~~~

Consider again the case where we have a scrollable page that contains an
iframe which itself is scrollable. As described above, we will have two
APZC instances - one for the page and one for the iframe. When the user
puts their finger down on the screen and moves it, we need to do some
sort of hit detection in order to determine whether their finger is on
the iframe or on the top-level page. Based on where their finger lands,
the appropriate APZC instance needs to handle the input. This hit
detection is also done in the APZCTreeManager, as it has the necessary
information about the sizes and positions of the layers. Currently this
hit detection is not perfect, as it uses rects and does not account for
things like rounded corners and opacity.

Also note that for some types of input (e.g. when the user puts two
fingers down to do a pinch) we do not want the input to be “split”
across two different APZC instances. In the case of a pinch, for
example, we find a “common ancestor” APZC instance - one that is
zoomable and contains all of the touch input points, and direct the
input to that APZC instance.

Scroll Handoff
~~~~~~~~~~~~~~

Consider yet again the case where we have a scrollable page that
contains an iframe which itself is scrollable. Say the user scrolls the
iframe so that it reaches the bottom. If the user continues panning on
the iframe, the expectation is that the top-level page will start
scrolling. However, as discussed in the section on hit detection, the
APZC instance for the iframe is separate from the APZC instance for the
top-level page. Thus, we need the two APZC instances to communicate in
some way such that input events on the iframe result in scrolling on the
top-level page. This behaviour is referred to as “scroll handoff” (or
“fling handoff” in the case where analogous behaviour results from the
scrolling momentum of the page after the user has lifted their finger).

Input event untransformation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The APZC architecture by definition results in two copies of a “scroll
position” for each scrollable layer. There is the original copy on the
main thread that is accessible to web content and the layout and
painting code. And there is a second copy on the compositor side, which
is updated asynchronously based on user input, and corresponds to what
the user visually sees on the screen. Although these two copies may
diverge temporarily, they are reconciled periodically. In particular,
they diverge while the APZ code is performing an async pan or zoom
action on behalf of the user, and are reconciled when the APZ code
requests a repaint from the main thread.

Because of the way input events are stored, this has some unfortunate
consequences. Input events are stored relative to the device screen - so
if the user touches at the same physical spot on the device, the same
input events will be delivered regardless of the content scroll
position. When the main thread receives a touch event, it combines that
with the content scroll position in order to figure out what DOM element
the user touched. However, because we now have two different scroll
positions, this process may not work perfectly. A concrete example
follows:

Consider a device with screen size 600 pixels tall. On this device, a
user is viewing a document that is 1000 pixels tall, and that is
scrolled down by 200 pixels. That is, the vertical section of the
document from 200px to 800px is visible. Now, if the user touches a
point 100px from the top of the physical display, the hardware will
generate a touch event with y=100. This will get sent to the main
thread, which will add the scroll position (200) and get a
document-relative touch event with y=300. This new y-value will be used
in hit detection to figure out what the user touched. If the document
had a absolute-positioned div at y=300, then that would receive the
touch event.

Now let us add some async scrolling to this example. Say that the user
additionally scrolls the document by another 10 pixels asynchronously
(i.e. only on the compositor thread), and then does the same touch
event. The same input event is generated by the hardware, and as before,
the document will deliver the touch event to the div at y=300. However,
visually, the document is scrolled by an additional 10 pixels so this
outcome is wrong. What needs to happen is that the APZ code needs to
intercept the touch event and account for the 10 pixels of asynchronous
scroll. Therefore, the input event with y=100 gets converted to y=110 in
the APZ code before being passed on to the main thread. The main thread
then adds the scroll position it knows about and determines that the
user touched at a document-relative position of y=310.

Analogous input event transformations need to be done for horizontal
scrolling and zooming.

Content independently adjusting scrolling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As described above, there are two copies of the scroll position in the
APZ architecture - one on the main thread and one on the compositor
thread. Usually for architectures like this, there is a single “source
of truth” value and the other value is simply a copy. However, in this
case that is not easily possible to do. The reason is that both of these
values can be legitimately modified. On the compositor side, the input
events the user is triggering modify the scroll position, which is then
propagated to the main thread. However, on the main thread, web content
might be running Javascript code that programmatically sets the scroll
position (via window.scrollTo, for example). Scroll changes driven from
the main thread are just as legitimate and need to be propagated to the
compositor thread, so that the visual display updates in response.

Because the cross-thread messaging is asynchronous, reconciling the two
types of scroll changes is a tricky problem. Our design solves this
using various flags and generation counters. The general heuristic we
have is that content-driven scroll position changes (e.g. scrollTo from
JS) are never lost. For instance, if the user is doing an async scroll
with their finger and content does a scrollTo in the middle, then some
of the async scroll would occur before the “jump” and the rest after the
“jump”.

Content preventing default behaviour of input events
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Another problem that we need to deal with is that web content is allowed
to intercept touch events and prevent the “default behaviour” of
scrolling. This ability is defined in web standards and is
non-negotiable. Touch event listeners in web content are allowed call
preventDefault() on the touchstart or first touchmove event for a touch
point; doing this is supposed to “consume” the event and prevent
touch-based panning. As we saw in a previous section, the input event
needs to be untransformed by the APZ code before it can be delivered to
content. But, because of the preventDefault problem, we cannot fully
process the touch event in the APZ code until content has had a chance
to handle it. Web browsers in general solve this problem by inserting a
delay of up to 300ms before processing the input - that is, web content
is allowed up to 300ms to process the event and call preventDefault on
it. If web content takes longer than 300ms, or if it completes handling
of the event without calling preventDefault, then the browser
immediately starts processing the events.

The way the APZ implementation deals with this is that upon receiving a
touch event, it immediately returns an untransformed version that can be
dispatched to content. It also schedules a 400ms timeout (600ms on
Android) during which content is allowed to prevent scrolling. There is
an API that allows the main-thread event dispatching code to notify the
APZ as to whether or not the default action should be prevented. If the
APZ content response timeout expires, or if the main-thread event
dispatching code notifies the APZ of the preventDefault status, then the
APZ continues with the processing of the events (which may involve
discarding the events).

The touch-action CSS property from the pointer-events spec is intended
to allow eliminating this 400ms delay in many cases (although for
backwards compatibility it will still be needed for a while). Note that
even with touch-action implemented, there may be cases where the APZ
code does not know the touch-action behaviour of the point the user
touched. In such cases, the APZ code will still wait up to 400ms for the
main thread to provide it with the touch-action behaviour information.

Technical details
-----------------

This section describes various pieces of the APZ code, and goes into
more specific detail on APIs and code than the previous sections. The
primary purpose of this section is to help people who plan on making
changes to the code, while also not going into so much detail that it
needs to be updated with every patch.

Overall flow of input events
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section describes how input events flow through the APZ code.

1.  Input events arrive from the hardware/widget code into the APZ via
    APZCTreeManager::ReceiveInputEvent. The thread that invokes this is
    called the input thread, and may or may not be the same as the Gecko
    main thread.
2.  Conceptually the first thing that the APZCTreeManager does is to
    associate these events with “input blocks”. An input block is a set
    of events that share certain properties, and generally are intended
    to represent a single gesture. For example with touch events, all
    events following a touchstart up to but not including the next
    touchstart are in the same block. All of the events in a given block
    will go to the same APZC instance and will either all be processed
    or all be dropped.
3.  Using the first event in the input block, the APZCTreeManager does a
    hit-test to see which APZC it hits. This hit-test uses the event
    regions populated on the layers, which may be larger than the true
    hit area of the layer. If no APZC is hit, the events are discarded
    and we jump to step 6. Otherwise, the input block is tagged with the
    hit APZC as a tentative target and put into a global APZ input
    queue.
4.

    i.  If the input events landed outside the dispatch-to-content event
        region for the layer, any available events in the input block
        are processed. These may trigger behaviours like scrolling or
        tap gestures.
    ii. If the input events landed inside the dispatch-to-content event
        region for the layer, the events are left in the queue and a
        400ms timeout is initiated. If the timeout expires before step 9
        is completed, the APZ assumes the input block was not cancelled
        and the tentative target is correct, and processes them as part
        of step 10.

5.  The call stack unwinds back to APZCTreeManager::ReceiveInputEvent,
    which does an in-place modification of the input event so that any
    async transforms are removed.
6.  The call stack unwinds back to the widget code that called
    ReceiveInputEvent. This code now has the event in the coordinate
    space Gecko is expecting, and so can dispatch it to the Gecko main
    thread.
7.  Gecko performs its own usual hit-testing and event dispatching for
    the event. As part of this, it records whether any touch listeners
    cancelled the input block by calling preventDefault(). It also
    activates inactive scrollframes that were hit by the input events.
8.  The call stack unwinds back to the widget code, which sends two
    notifications to the APZ code on the input thread. The first
    notification is via APZCTreeManager::ContentReceivedInputBlock, and
    informs the APZ whether the input block was cancelled. The second
    notification is via APZCTreeManager::SetTargetAPZC, and informs the
    APZ of the results of the Gecko hit-test during event dispatch. Note
    that Gecko may report that the input event did not hit any
    scrollable frame at all. The SetTargetAPZC notification happens only
    once per input block, while the ContentReceivedInputBlock
    notification may happen once per block, or multiple times per block,
    depending on the input type.
9.

    i.   If the events were processed as part of step 4(i), the
         notifications from step 8 are ignored and step 10 is skipped.
    ii.  If events were queued as part of step 4(ii), and steps 5-8 take
         less than 400ms, the arrival of both notifications from step 8
         will mark the input block ready for processing.
    iii. If events were queued as part of step 4(ii), but steps 5-8 take
         longer than 400ms, the notifications from step 8 will be
         ignored and step 10 will already have happened.

10. If events were queued as part of step 4(ii) they are now either
    processed (if the input block was not cancelled and Gecko detected a
    scrollframe under the input event, or if the timeout expired) or
    dropped (all other cases). Note that the APZC that processes the
    events may be different at this step than the tentative target from
    step 3, depending on the SetTargetAPZC notification. Processing the
    events may trigger behaviours like scrolling or tap gestures.

If the CSS touch-action property is enabled, the above steps are
modified as follows: \* In step 4, the APZC also requires the allowed
touch-action behaviours for the input event. This might have been
determined as part of the hit-test in APZCTreeManager; if not, the
events are queued. \* In step 6, the widget code determines the content
element at the point under the input element, and notifies the APZ code
of the allowed touch-action behaviours. This notification is sent via a
call to APZCTreeManager::SetAllowedTouchBehavior on the input thread. \*
In step 9(ii), the input block will only be marked ready for processing
once all three notifications arrive.

Threading considerations
^^^^^^^^^^^^^^^^^^^^^^^^

The bulk of the input processing in the APZ code happens on what we call
“the input thread”. In practice the input thread could be the Gecko main
thread, the compositor thread, or some other thread. There are obvious
downsides to using the Gecko main thread - that is, “asynchronous”
panning and zooming is not really asynchronous as input events can only
be processed while Gecko is idle. In an e10s environment, using the
Gecko main thread of the chrome process is acceptable, because the code
running in that process is more controllable and well-behaved than
arbitrary web content. Using the compositor thread as the input thread
could work on some platforms, but may be inefficient on others. For
example, on Android (Fennec) we receive input events from the system on
a dedicated UI thread. We would have to redispatch the input events to
the compositor thread if we wanted to the input thread to be the same as
the compositor thread. This introduces a potential for higher latency,
particularly if the compositor does any blocking operations - blocking
SwapBuffers operations, for example. As a result, the APZ code itself
does not assume that the input thread will be the same as the Gecko main
thread or the compositor thread.

Active vs. inactive scrollframes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The number of scrollframes on a page is potentially unbounded. However,
we do not want to create a separate layer for each scrollframe right
away, as this would require large amounts of memory. Therefore,
scrollframes as designated as either “active” or “inactive”. Active
scrollframes are the ones that do have their contents put on a separate
layer (or set of layers), and inactive ones do not.

Consider a page with a scrollframe that is initially inactive. When
layout generates the layers for this page, the content of the
scrollframe will be flattened into some other PaintedLayer (call it P).
The layout code also adds the area (or bounding region in case of weird
shapes) of the scrollframe to the dispatch-to-content region of P.

When the user starts interacting with that content, the hit-test in the
APZ code finds the dispatch-to-content region of P. The input block
therefore has a tentative target of P when it goes into step 4(ii) in
the flow above. When gecko processes the input event, it must detect the
inactive scrollframe and activate it, as part of step 7. Finally, the
widget code sends the SetTargetAPZC notification in step 8 to notify the
APZ that the input block should really apply to this new layer. The
issue here is that the layer transaction containing the new layer must
reach the compositor and APZ before the SetTargetAPZC notification. If
this does not occur within the 400ms timeout, the APZ code will be
unable to update the tentative target, and will continue to use P for
that input block. Input blocks that start after the layer transaction
will get correctly routed to the new layer as there will now be a layer
and APZC instance for the active scrollframe.

This model implies that when the user initially attempts to scroll an
inactive scrollframe, it may end up scrolling an ancestor scrollframe.
(This is because in the absence of the SetTargetAPZC notification, the
input events will get applied to the closest ancestor scrollframe’s
APZC.) Only after the round-trip to the gecko thread is complete is
there a layer for async scrolling to actually occur on the scrollframe
itself. At that point the scrollframe will start receiving new input
blocks and will scroll normally.

WebRender Integration
~~~~~~~~~~~~~~~~~~~~~

The APZ code was originally written to work with the "layers" graphics
backend. Many of the concepts (and therefore variable/function names)
stem from the integration with the layers backend. After the WebRender
backend was added, the existing code evolved over time to integrate
with that backend as well, resulting in a bit of a hodge-podge effect.
With that cautionary note out of the way, there are three main pieces
that need to be understood to grasp the integration between the APZ
code and WebRender. These are detailed below.

HitTestingTree
^^^^^^^^^^^^^^

The APZCTreeManager keeps as part of its internal state a tree of
HitTestingTreeNode instances. This is referred to as the HitTestingTree.
As the name implies, this was used for hit-testing purposes, so that
APZ could determine which scrollframe a particular incoming input event
would be targeting. Doing the hit-test requires access to a bunch of state,
such as CSS transforms and clip rects, as well as ancillary data like
event regions, which affect how APZ reacts to input events.

With the layers backend, all this information was provided by a layer tree
update, and so the HitTestingTree was created to mirror the layer tree,
allowing APZ access to that information from other threads. The structure
of the tree was identical to the layer tree. But with WebRender, there
is no "layer tree" per se, and instead we "fake it" by creating a
HitTestingTree structure that is similar to what it would be like on the
equivalent layer tree. But the bigger difference is that with WebRender,
the HitTestingTree is not actually used for hit-testing at all; instead
we get WebRender to do the hit-test for us, as it can do so using its
own internal state and produce a more precise result.

Information stored in the HitTestingTree (e.g. CSS transforms) is still
used by other pieces of APZ (e.g. some of the scrollbar manipulation code)
so it is still needed, even with the WebRender backend. For this reason,
and for consistency between the two backends, we try to populate as much
information in the HitTestingTree that we can, even with the WebRender
backend.

With the layers backend, the way the HitTestingTree is created is by
walking the layer tree with a LayerMetricsWrapper class. This wraps
a layer tree but also expands layers with multiple ScrollMetadata into
multiple nodes. The equivalent in the WebRender world is the
WebRenderScrollDataWrapper, which wraps a WebRenderScrollData object. The
WebRenderScrollData object is roughly analogous to a layer tree, but
is something that is constructed deliberately rather than being a natural
output from the WebRender paint transaction (i.e. we create it explicitly
for APZ's consumption, rather than something that we would create anyway
for WebRender's consumption).

The WebRenderScrollData structure contains within it a tree of
WebRenderLayerScrollData instances, which are analogous to individual
layers in a layer tree. These instances contain various fields like
CSS transforms, fixed/sticky position info, etc. that would normally be
found on individual layers in the layer tree. This allows the code
that builds the HitTestingTree to consume either a WebRenderScrollData
or a layer tree in a more-or-less unified fashion.

Working backwards a bit more, the WebRenderLayerScrollData instances
are created as we traverse the Gecko display list and build the
WebRender display list. In the layers world, the code in FrameLayerBuilder
was responsible for building the layer tree from the Gecko display list,
but in the WebRender world, this happens primarily in WebRenderCommandBuilder.
As of this writing, the architecture for this is that, as we walk
the Gecko display list, we query it to see if it contains any information
that APZ might need to know (e.g. CSS transforms) via a call to
`nsDisplayItem::UpdateScrollData(nullptr, nullptr)`. If this call
returns true, we create a WebRenderLayerScrollData instance for the item,
and populate it with the necessary information in
`WebRenderLayerScrollData::Initialize`. We also create
WebRenderLayerScrollData instances if we detect (via ASR changes) that
we are now processing a Gecko display item that is in a different scrollframe
than the previous item. This is equivalent to how FrameLayerBuilder will
flatten items with different ASRs into different layers, so that it
is cheap to scroll scrollframes in the compositor.

The main sources of complexity in this code come from:

1. Ensuring the ScrollMetadata instances end on the proper
   WebRenderLayerScrollData instances (such that every path from a leaf
   WebRenderLayerScrollData node to the root has a consistent ordering of
   scrollframes without duplications).
2. The deferred-transform optimization that is described in more detail
   at the declaration of StackingContextHelper::mDeferredTransformItem.

Hit-testing
^^^^^^^^^^^

Since the HitTestingTree is not used for actual hit-testing purposes
with the WebRender backend (see previous section), this section describes
how hit-testing actually works with WebRender.

With both layers and WebRender, the Gecko display list contains display items
(`nsDisplayCompositorHitTestInfo`) that store hit-testing state. These
items implement the `CreateWebRenderCommands` method and generate a "hit-test
item" into the WebRender display list. This is basically just a rectangle
item in the WebRender display list that is no-op for painting purposes,
but contains information that should be returned by the hit-test (specifically
the hit info flags and the scrollId of the enclosing scrollframe). The
hit-test item gets clipped and transformed in the same way that all the other
items in the WebRender display list do, via clip chains and enclosing
reference frame/stacking context items.

When WebRender needs to do a hit-test, it goes through its display list,
taking into account the current clips and transforms, adjusted for the
most recent async scroll/zoom, and determines which hit-test item(s) are under
the target point, and returns those items. APZ can then take the frontmost
item from that list (or skip over it if it happens to be inside a OOP
subdocument that's pointer-events:none) and use that as the hit target.
It's important to note that when APZ does hit-testing for the layers backend,
it uses the most recent available async transforms, even if those transforms
have not yet been composited. With WebRender, the hit-test uses the last
transform provided by the `SampleForWebRender` API (see next section) which
generally reflects the last composite, and doesn't take into account further
changes to the transforms that have occurred since then.

When debugging hit-test issues, it is often useful to apply the patches
on bug 1656260, which introduce a guid on Gecko display items and propagate
it all the way through to where APZ gets the hit-test result. This allows
answering the question "which nsDisplayCompositorHitTestInfo was responsible
for this hit-test result?" which is often a very good first step in
solving the bug. From there, one can determine if there was some other
display item in front that should have generated a
nsDisplayCompositorHitTestInfo but didn't, or if display item itself had
incorrect information. The second patch on that bug further allows exposing
hand-written debug info to the APZ code, so that the WR hit-testing
mechanism itself can be more effectively debugged, in case there is a problem
with the WR display items getting improperly transformed or clipped.

Sampling
^^^^^^^^

With both the layers and WebRender backend, the compositing step needs to
read the latest async transforms from APZ in order to ensure scrollframes
are rendered at the right position. In both cases, the API for this is
exposed via the `APZSampler` class. The difference is that with the layers
backend, the `AsyncCompositionManager` walks the layer tree and queries
the transform components for each layer individually via the various getters
on `APZSampler`. In contrast, with the WebRender backend, there is a single
`APZSampler::SampleForWebRender` API that returns all the information needed
for all the scrollframes, scrollthumbs, etc. Conceptually though, the
functionality is pretty similar, because the compositor needs the same
information from APZ regardless of which backend is in use.

Along with sampling the APZ transforms, the compositor also triggers APZ
animations to advance to the next timestep (usually the next vsync). Again,
with both the WebRender and layers backend, this happens just before reading
the APZ transforms. The only difference is that with the layers backend,
the `AsyncCompositionManager` invokes the `APZSampler::AdvanceAnimations` API
directly, whereas with the WebRender backend this happens as part of the
`APZSampler::SampleForWebRender` implementation.

Threading / Locking Overview
----------------------------

Threads
~~~~~~~

There are three threads relevant to APZ: the **controller thread**,
the **updater thread**, and the **sampler thread**. This table lists
which threads play these roles on each platform / configuration:

===================== ========== =========== ============= ============== ========== =============
APZ Thread Name       Desktop    Desktop+GPU Desktop+WR    Desktop+WR+GPU Android    Android+WR
===================== ========== =========== ============= ============== ========== =============
**controller thread** UI main    GPU main    UI main       GPU main       Java UI    Java UI
**updater thread**    Compositor Compositor  SceneBuilder  SceneBuilder   Compositor SceneBuilder
**sampler thread**    Compositor Compositor  RenderBackend RenderBackend  Compositor RenderBackend
===================== ========== =========== ============= ============== ========== =============

Locks
~~~~~

There are also a number of locks used in APZ code:

======================= ==============================
Lock type               How many instances
======================= ==============================
APZ tree lock           one per APZCTreeManager
APZC map lock           one per APZCTreeManager
APZC instance lock      one per AsyncPanZoomController
APZ test lock           one per APZCTreeManager
Checkerboard event lock one per AsyncPanZoomController
======================= ==============================

Thread / Lock Ordering
~~~~~~~~~~~~~~~~~~~~~~

To avoid deadlocks, the threads and locks have a global **ordering**
which must be respected.

Respecting the ordering means the following:

- Let "A < B" denote that A occurs earlier than B in the ordering
- Thread T may only acquire lock L, if T < L
- A thread may only acquire lock L2 while holding lock L1, if L1 < L2
- A thread may only block on a response from another thread T while holding a lock L, if L < T

**The lock ordering is as follows**:

1. UI main
2. GPU main              (only if GPU enabled)
3. Compositor thread
4. SceneBuilder thread   (only if WR enabled)
5. **APZ tree lock**
6. RenderBackend thread  (only if WR enabled)
7. **APZC map lock**
8. **APZC instance lock**
9. **APZ test lock**
10. **Checkerboard event lock**

Example workflows
^^^^^^^^^^^^^^^^^

Here are some example APZ workflows. Observe how they all obey
the global thread/lock ordering. Feel free to add others:

- **Input handling** (in WR+GPU) case: UI main -> GPU main -> APZ tree lock -> RenderBackend thread
- **Sync messages** in ``PCompositorBridge.ipdl``: UI main thread -> Compositor thread
- **GetAPZTestData**: Compositor thread -> SceneBuilder thread -> test lock
- **Scene swap**: SceneBuilder thread -> APZ tree lock -> RenderBackend thread
- **Updating hit-testing tree**: SceneBuilder thread -> APZ tree lock -> APZC instance lock
- **Updating APZC map**: SceneBuilder thread -> APZ tree lock -> APZC map lock
- **Sampling and animation deferred tasks** [1]_: RenderBackend thread -> APZC map lock -> APZC instance lock
- **Advancing animations**: RenderBackend thread -> APZC instance lock

.. [1] It looks like there are two deferred tasks that actually need the tree lock,
   ``AsyncPanZoomController::HandleSmoothScrollOverscroll`` and
   ``AsyncPanZoomController::HandleFlingOverscroll``. We should be able to rewrite
   these to use the map lock instead of the tree lock.
   This will allow us to continue running the deferred tasks on the sampler
   thread rather than having to bounce them to another thread.
