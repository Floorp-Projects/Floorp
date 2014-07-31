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
Our design solves this using various flags and generation counters that ensures compositor-driven scroll changes never overwrite content-driven scroll changes - this behaviour is usually the desired outcome.

### Content preventing default behaviour of input events

Another problem that we need to deal with is that web content is allowed to intercept touch events and prevent the "default behaviour" of scrolling.
This ability is defined in web standards and is non-negotiable.
Touch event listeners in web content are allowed call preventDefault() on the touchstart or first touchmove event for a touch point; doing this is supposed to "consume" the event and prevent touch-based panning.
As we saw in a previous section, the input event needs to be untransformed by the APZ code before it can be delivered to content.
But, because of the preventDefault problem, we cannot fully process the touch event in the APZ code until content has had a chance to handle it.
Web browsers in general solve this problem by inserting a delay of up to 300ms before processing the input - that is, web content is allowed up to 300ms to process the event and call preventDefault on it.
If web content takes longer than 300ms, or if it completes handling of the event without calling preventDefault, then the browser immediately starts processing the events.
The touch-action CSS property from the pointer-events spec is intended to allow eliminating this 300ms delay, although for backwards compatibility it will still be needed for a while.

The way the APZ implementation deals with this is that upon receiving a touch event, it immediately returns an untransformed version that can be dispatched to content.
It also schedules a 300ms timeout during which content is allowed to prevent scrolling.
There is an API that allows the main-thread event dispatching code to notify the APZ as to whether or not the default action should be prevented.
If the APZ content response timeout expires, or if the main-thread event dispatching code notifies the APZ of the preventDefault status, then the APZ continues with the processing of the events (which may involve discarding the events).
