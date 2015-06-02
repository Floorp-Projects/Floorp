Asynchronous Panning and Zooming {#apz}
================================

**This document is a work in progress.  Some information may be missing or incomplete.**

## Goals

We need to be able to provide a visual response to user input with minimal latency.
In particular, on devices with touch input, content must track the finger exactly while panning, or the user experience is very poor.
According to the UX team, 120ms is an acceptable latency between user input and response.

## Context and surrounding architecture

The fundamental problem we are trying to solve with the Asynchronous Panning and Zooming (APZ) code is that of responsiveness.
By default, web browsers operate in a "game loop" that looks like this:

    while true:
        process input
        do computations
        repaint content
        display repainted content

In browsers the "do computation" step can be arbitrarily expensive because it can involve running event handlers in web content.
Therefore, there can be an arbitrary delay between the input being received and the on-screen display getting updated.

Responsiveness is always good, and with touch-based interaction it is even more important than with mouse or keyboard input.
In order to ensure responsiveness, we split the "game loop" model of the browser into a multithreaded variant which looks something like this:

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

This multithreaded model is called off-main-thread compositing (OMTC), because the compositing (where the content is displayed on-screen) happens on a separate thread from the main thread.
Note that this is a very very simplified model, but in this model the "adjust painted content based on input" is the primary function of the APZ code.

The "painted content" is stored on a set of "layers", that are conceptually double-buffered.
That is, when the main thread does its repaint, it paints into one set of layers (the "client" layers).
The update that is sent to the compositor thread copies all the changes from the client layers into another set of layers that the compositor holds.
These layers are called the "shadow" layers or the "compositor" layers.
The compositor in theory can continuously composite these shadow layers to the screen while the main thread is busy doing other things and painting a new set of client layers.

The APZ code takes the input events that are coming in from the hardware and uses them to figure out what the user is trying to do (e.g. pan the page, zoom in).
It then expresses this user intention in the form of translation and/or scale transformation matrices.
These transformation matrices are applied to the shadow layers at composite time, so that what the user sees on-screen reflects what they are trying to do as closely as possible.

## Technical overview

As per the heavily simplified model described above, the fundamental purpose of the APZ code is to take input events and produce transformation matrices.
This section attempts to break that down and identify the different problems that make this task non-trivial.

### Checkerboarding

The content area that is painted and stored in a shadow layer is called the "displayport".
The APZ code is responsible for determining how large the displayport should be.
On the one hand, we want the displayport to be as large as possible.
At the very least it needs to be larger than what is visible on-screen, because otherwise, as soon as the user pans, there will be some unpainted area of the page exposed.
However, we cannot always set the displayport to be the entire page, because the page can be arbitrarily long and this would require an unbounded amount of memory to store.
Therefore, a good displayport size is one that is larger than the visible area but not so large that it is a huge drain on memory.
Because the displayport is usually smaller than the whole page, it is always possible for the user to scroll so fast that they end up in an area of the page outside the displayport.
When this happens, they see unpainted content; this is referred to as "checkerboarding", and we try to avoid it where possible.

There are many possible ways to determine what the displayport should be in order to balance the tradeoffs involved (i.e. having one that is too big is bad for memory usage, and having one that is too small results in excessive checkerboarding).
Ideally, the displayport should cover exactly the area that we know the user will make visible.
Although we cannot know this for sure, we can use heuristics based on current panning velocity and direction to ensure a reasonably-chosen displayport area.
This calculation is done in the APZ code, and a new desired displayport is frequently sent to the main thread as the user is panning around.

### Multiple layers

Consider, for example, a scrollable page that contains an iframe which itself is scrollable.
The iframe can be scrolled independently of the top-level page, and we would like both the page and the iframe to scroll responsively.
This means that we want independent asynchronous panning for both the top-level page and the iframe.
In addition to iframes, elements that have the overflow:scroll CSS property set are also scrollable, and also end up on separate scrollable layers.
In the general case, the layers are arranged in a tree structure, and so within the APZ code we have a matching tree of AsyncPanZoomController (APZC) objects, one for each scrollable layer.
To manage this tree of APZC instances, we have a single APZCTreeManager object.
Each APZC is relatively independent and handles the scrolling for its associated layer, but there are some cases in which they need to interact; these cases are described in the sections below.

### Hit detection

Consider again the case where we have a scrollable page that contains an iframe which itself is scrollable.
As described above, we will have two APZC instances - one for the page and one for the iframe.
When the user puts their finger down on the screen and moves it, we need to do some sort of hit detection in order to determine whether their finger is on the iframe or on the top-level page.
Based on where their finger lands, the appropriate APZC instance needs to handle the input.
This hit detection is also done in the APZCTreeManager, as it has the necessary information about the sizes and positions of the layers.
Currently this hit detection is not perfect, as it uses rects and does not account for things like rounded corners and opacity.

Also note that for some types of input (e.g. when the user puts two fingers down to do a pinch) we do not want the input to be "split" across two different APZC instances.
In the case of a pinch, for example, we find a "common ancestor" APZC instance - one that is zoomable and contains all of the touch input points, and direct the input to that APZC instance.

### Scroll Handoff

Consider yet again the case where we have a scrollable page that contains an iframe which itself is scrollable.
Say the user scrolls the iframe so that it reaches the bottom.
If the user continues panning on the iframe, the expectation is that the top-level page will start scrolling.
However, as discussed in the section on hit detection, the APZC instance for the iframe is separate from the APZC instance for the top-level page.
Thus, we need the two APZC instances to communicate in some way such that input events on the iframe result in scrolling on the top-level page.
This behaviour is referred to as "scroll handoff" (or "fling handoff" in the case where analogous behaviour results from the scrolling momentum of the page after the user has lifted their finger).

### Input event untransformation

The APZC architecture by definition results in two copies of a "scroll position" for each scrollable layer.
There is the original copy on the main thread that is accessible to web content and the layout and painting code.
And there is a second copy on the compositor side, which is updated asynchronously based on user input, and corresponds to what the user visually sees on the screen.
Although these two copies may diverge temporarily, they are reconciled periodically.
In particular, they diverge while the APZ code is performing an async pan or zoom action on behalf of the user, and are reconciled when the APZ code requests a repaint from the main thread.

Because of the way input events are stored, this has some unfortunate consequences.
Input events are stored relative to the device screen - so if the user touches at the same physical spot on the device, the same input events will be delivered regardless of the content scroll position.
When the main thread receives a touch event, it combines that with the content scroll position in order to figure out what DOM element the user touched.
However, because we now have two different scroll positions, this process may not work perfectly.
A concrete example follows:

Consider a device with screen size 600 pixels tall.
On this device, a user is viewing a document that is 1000 pixels tall, and that is scrolled down by 200 pixels.
That is, the vertical section of the document from 200px to 800px is visible.
Now, if the user touches a point 100px from the top of the physical display, the hardware will generate a touch event with y=100.
This will get sent to the main thread, which will add the scroll position (200) and get a document-relative touch event with y=300.
This new y-value will be used in hit detection to figure out what the user touched.
If the document had a absolute-positioned div at y=300, then that would receive the touch event.

Now let us add some async scrolling to this example.
Say that the user additionally scrolls the document by another 10 pixels asynchronously (i.e. only on the compositor thread), and then does the same touch event.
The same input event is generated by the hardware, and as before, the document will deliver the touch event to the div at y=300.
However, visually, the document is scrolled by an additional 10 pixels so this outcome is wrong.
What needs to happen is that the APZ code needs to intercept the touch event and account for the 10 pixels of asynchronous scroll.
Therefore, the input event with y=100 gets converted to y=110 in the APZ code before being passed on to the main thread.
The main thread then adds the scroll position it knows about and determines that the user touched at a document-relative position of y=310.

Analogous input event transformations need to be done for horizontal scrolling and zooming.

### Content independently adjusting scrolling

As described above, there are two copies of the scroll position in the APZ architecture - one on the main thread and one on the compositor thread.
Usually for architectures like this, there is a single "source of truth" value and the other value is simply a copy.
However, in this case that is not easily possible to do.
The reason is that both of these values can be legitimately modified.
On the compositor side, the input events the user is triggering modify the scroll position, which is then propagated to the main thread.
However, on the main thread, web content might be running Javascript code that programatically sets the scroll position (via window.scrollTo, for example).
Scroll changes driven from the main thread are just as legitimate and need to be propagated to the compositor thread, so that the visual display updates in response.

Because the cross-thread messaging is asynchronous, reconciling the two types of scroll changes is a tricky problem.
Our design solves this using various flags and generation counters.
The general heuristic we have is that content-driven scroll position changes (e.g. scrollTo from JS) are never lost.
For instance, if the user is doing an async scroll with their finger and content does a scrollTo in the middle, then some of the async scroll would occur before the "jump" and the rest after the "jump".

### Content preventing default behaviour of input events

Another problem that we need to deal with is that web content is allowed to intercept touch events and prevent the "default behaviour" of scrolling.
This ability is defined in web standards and is non-negotiable.
Touch event listeners in web content are allowed call preventDefault() on the touchstart or first touchmove event for a touch point; doing this is supposed to "consume" the event and prevent touch-based panning.
As we saw in a previous section, the input event needs to be untransformed by the APZ code before it can be delivered to content.
But, because of the preventDefault problem, we cannot fully process the touch event in the APZ code until content has had a chance to handle it.
Web browsers in general solve this problem by inserting a delay of up to 300ms before processing the input - that is, web content is allowed up to 300ms to process the event and call preventDefault on it.
If web content takes longer than 300ms, or if it completes handling of the event without calling preventDefault, then the browser immediately starts processing the events.

The way the APZ implementation deals with this is that upon receiving a touch event, it immediately returns an untransformed version that can be dispatched to content.
It also schedules a 300ms timeout during which content is allowed to prevent scrolling.
There is an API that allows the main-thread event dispatching code to notify the APZ as to whether or not the default action should be prevented.
If the APZ content response timeout expires, or if the main-thread event dispatching code notifies the APZ of the preventDefault status, then the APZ continues with the processing of the events (which may involve discarding the events).

The touch-action CSS property from the pointer-events spec is intended to allow eliminating this 300ms delay in many cases (although for backwards compatibility it will still be needed for a while).
Note that even with touch-action implemented, there may be cases where the APZ code does not know the touch-action behaviour of the point the user touched.
In such cases, the APZ code will still wait up to 300ms for the main thread to provide it with the touch-action behaviour information.

## Technical details

This section describes various pieces of the APZ code, and goes into more specific detail on APIs and code than the previous sections.
The primary purpose of this section is to help people who plan on making changes to the code, while also not going into so much detail that it needs to be updated with every patch.

### Overall flow of input events

This section describes how input events flow through the APZ code.
<ol>
<li value="1">
Input events arrive from the hardware/widget code into the APZ via APZCTreeManager::ReceiveInputEvent.
The thread that invokes this is called the input thread, and may or may not be the same as the Gecko main thread.
</li>
<li value="2">
Conceptually the first thing that the APZCTreeManager does is to group these events into "input blocks".
An input block is a contiguous set of events that get handled together.
For example with touch events, all events following a touchstart up to but not including the next touchstart are in the same block.
All of the events in a given block will go to the same APZC instance and will either all be processed or all be dropped.
</li>
<li value="3">
Using the first event in the input block, the APZCTreeManager does a hit-test to see which APZC it hits.
This hit-test uses the event regions populated on the layers, which may be larger than the true hit area of the layer.
If no APZC is hit, the events are discarded and we jump to step 6.
Otherwise, the input block is tagged with the hit APZC as a tentative target and put into a global APZ input queue.
</li>
<li value="4">
 <ol>
  <li value="i">
   If the input events landed outside the dispatch-to-content event region for the layer, any available events in the input block are processed.
   These may trigger behaviours like scrolling or tap gestures.
  </li>
  <li value="ii">
   If the input events landed inside the dispatch-to-content event region for the layer, the events are left in the queue and a 300ms timeout is initiated.
   If the timeout expires before step 9 is completed, the APZ assumes the input block was not cancelled and the tentative target is correct, and processes them as part of step 10.
  </li>
 </ol>
</li>
<li value="5">
The call stack unwinds back to APZCTreeManager::ReceiveInputEvent, which does an in-place modification of the input event so that any async transforms are removed.
</li>
<li value="6">
The call stack unwinds back to the widget code that called ReceiveInputEvent.
This code now has the event in the coordinate space Gecko is expecting, and so can dispatch it to the Gecko main thread.
</li>
<li value="7">
Gecko performs its own usual hit-testing and event dispatching for the event.
As part of this, it records whether any touch listeners cancelled the input block by calling preventDefault().
It also activates inactive scrollframes that were hit by the input events.
</li>
<li value="8">
The call stack unwinds back to the widget code, which sends two notifications to the APZ code on the input thread.
The first notification is via APZCTreeManager::ContentReceivedInputBlock, and informs the APZ whether the input block was cancelled.
The second notification is via APZCTreeManager::SetTargetAPZC, and informs the APZ the results of the Gecko hit-test during event dispatch.
Note that Gecko may report that the input event did not hit any scrollable frame at all.
These notifications happen only once per input block.
</li>
<li value="9">
 <ol>
  <li value="i">
   If the events were processed as part of step 4(i), the notifications from step 8 are ignored and step 10 is skipped.
  </li>
  <li value="ii">
   If events were queued as part of step 4(ii), and steps 5-8 take less than 300ms, the arrival of both notifications from step 8 will mark the input block ready for processing.
  </li>
  <li value="iii">
   If events were queued as part of step 4(ii), but steps 5-8 take longer than 300ms, the notifications from step 8 will be ignored and step 10 will already have happened.
  </li>
 </ol>
</li>
<li value="10">
If events were queued as part of step 4(ii) they are now either processed (if the input block was not cancelled and Gecko detected a scrollframe under the input event, or if the timeout expired) or dropped (all other cases).
Note that the APZC that processes the events may be different at this step than the tentative target from step 3, depending on the SetTargetAPZC notification.
Processing the events may trigger behaviours like scrolling or tap gestures.
</li>
</ol>

If the CSS touch-action property is enabled, the above steps are modified as follows:
<ul>
<li>
 In step 4, the APZC also requires the allowed touch-action behaviours for the input event. This is not available yet, so the events are always queued.
</li>
<li>
 In step 6, the widget code determines the content element at the point under the input element, and notifies the APZ code of the allowed touch-action behaviours.
 This notification is sent via a call to APZCTreeManager::SetAllowedTouchBehavior on the input thread.
</li>
<li>
 In step 9(ii), the input block will only be marked ready for processing once all three notifications arrive.
</li>
</ul>

#### Threading considerations

The bulk of the input processing in the APZ code happens on what we call "the input thread".
In practice the input thread could be the Gecko main thread, the compositor thread, or some other thread.
There are obvious downsides to using the Gecko main thread - that is, "asynchronous" panning and zooming is not really asynchronous as input events can only be processed while Gecko is idle.
However, this is the current state of things on B2G.
Using the compositor thread as the input thread could work on some platforms, but may be inefficient on others.
For example, on Android (Fennec) we receive input events from the system on a dedicated UI thread.
We would have to redispatch the input events to the compositor thread if we wanted to the input thread to be the same as the compositor thread.
This introduces a potential for higher latency, particularly if the compositor does any blocking operations - blocking SwapBuffers operations, for example.
As a result, the APZ code itself does not assume that the input thread will be the same as the Gecko main thread or the compositor thread.

#### Active vs. inactive scrollframes

The number of scrollframes on a page is potentially unbounded.
However, we do not want to create a separate layer for each scrollframe right away, as this would require large amounts of memory.
Therefore, scrollframes as designated as either "active" or "inactive".
Active scrollframes are the ones that do have their contents put on a separate layer (or set of layers), and inactive ones do not.

Consider a page with a scrollframe that is initially inactive.
When layout generates the layers for this page, the content of the scrollframe will be flattened into some other PaintedLayer (call it P).
The layout code also adds the area (or bounding region in case of weird shapes) of the scrollframe to the dispatch-to-content region of P.

When the user starts interacting with that content, the hit-test in the APZ code finds the dispatch-to-content region of P.
The input block therefore has a tentative target of P when it goes into step 4(ii) in the flow above.
When gecko processes the input event, it must detect the inactive scrollframe and activate it, as part of step 7.
Finally, the widget code sends the SetTargetAPZC notification in step 8 to notify the APZ that the input block should really apply to this new layer.
The issue here is that the layer transaction containing the new layer must reach the compositor and APZ before the SetTargetAPZC notification.
If this does not occur within the 300ms timeout, the APZ code will be unable to update the tentative target, and will continue to use P for that input block.
Input blocks that start after the layer transaction will get correctly routed to the new layer as there will now be a layer and APZC instance for the active scrollframe.

This model implies that when the user initially attempts to scroll an inactive scrollframe, it may end up scrolling an ancestor scrollframe.
(This is because in the absence of the SetTargetAPZC notification, the input events will get applied to the closest ancestor scrollframe's APZC.)
Only after the round-trip to the gecko thread is complete is there a layer for async scrolling to actually occur on the scrollframe itself.
At that point the scrollframe will start receiving new input blocks and will scroll normally.
