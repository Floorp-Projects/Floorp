.. _docsplit:

Document Splitting in WebRender
===============================

Goals
-----

The fundamental goal of document splitting is to fix a specific performance issue.
The architecture of WebRender is such that any time a content process sends a new
display list to the compositor, WebRender needs to do a full scene build that
includes both the content and chrome areas. Likewise if the chrome process sends
a new display list to the compositor. This means that animations such as the
tab loading spinner or an animated gif will cause much more WR activity than
is really needed.

With document splitting, the WR scene is split into two (or more) documents
based on the visual location of the elements. Everything in the "chrome area"
of the window (including the tab bar, URL bar, navigation buttons, etc.) are
considered part of one document, and anything below that (including content,
devtools, sidebars, etc.) is in another document. Bug 1549976 introduces a
third "popover" document that encompasses elements that straddle both of these
visually, but that's a side-effect of the implementation rather than being
driven by the fundamental problem being addressed.

With the documents split like so, an animation in the UI (such as the tab loading
spinner) runs independently of the content (by virtue of being in a different
document) and vice-versa, which results in better user-perceived performance.

Naming
------

Document splitting is so called because inside the WR code and bindings, a
"document" is an independent pathway to the compositor that has its own scene,
render tasks, etc.

In most of the C++ code in gfx/layers and other parts of Firefox/Gecko, the term
"render root" is used instead. A "render root" is exactly equivalent to a WR
document; the two terms are used interchangeably. The naming is this way
because "document" has different pre-existing meanings in Gecko-land. In this
documentation those other meanings are irrelevant and so "document" always refers
to the same thing as "render root".

At various points there have been discussions to renaming things so that everything
is more consistent and less confusing, but that hasn't happened yet.

Fundamental data types
----------------------

The `wr::RenderRoot
<https://searchfox.org/mozilla-central/rev/da14c413ef663eb1ba246799e94a240f81c42488/gfx/webrender_bindings/WebRenderTypes.h#65>`_
data type is an enumeration of all the different documents that are expected
to be created. As of this writing, the enumeration contains two entries: Default
and Content. The Default document refers to the one that holds the "chrome" stuff
and the Content document refers to the one that holds the "content" area stuff.

If document splitting is disabled (gfx.webrender.split-render-roots=false) then
everything lives in the Default document and the Content document is always empty.

Additional data structures in the same file (e.g. wr::RenderRootArray<T>) facilitate
converting pre-document-splitting code into document-splitting-aware code, usually
by turning a single object into an array of objects, one per document.

High-level design
-----------------

The notion of having multiple documents has to be introduced at a fairly fundamental
level in order to be propagated through the entire rendering pipeline. It starts
in the front-end HTML/XUL code where certain elements are annotated as being the
transition point between documents. For example, `this code
<https://searchfox.org/mozilla-central/rev/8ed8474757695cdae047150a0eaf94a5f1c96dbe/browser/base/content/browser.xhtml#1304>`_
explicitly identifies an element and its descendants as being in the "content"
document instead of the "default" (or "chrome") document.

These attributes are `read during display list building
<https://searchfox.org/mozilla-central/rev/8ed8474757695cdae047150a0eaf94a5f1c96dbe/layout/xul/nsBoxFrame.cpp#1112>`_
to create the nsDisplayRenderRoot display item in the Gecko display list.

When the Gecko display list is processed to create a WebRender display list,
it actually ends up creating multiple WR display lists, one for each document. This
is necessary because the documents are handled independently inside WR, and so
each get their own WebRenderAPI object and separate display list. The way the
implementation manages this is by `creating a "sub builder"
<https://searchfox.org/mozilla-central/rev/8ed8474757695cdae047150a0eaf94a5f1c96dbe/layout/painting/nsDisplayList.cpp#7043,7065>`_
for the render root that is being descended into, and using that instead of the
main WR display list builder as the display list is recursed into.

Note also that clip chains and stacking contexts are per-document, so when
recursing past a nsDisplayRenderRoot item, the `ClipManager and StackingContextHelper
<https://searchfox.org/mozilla-central/rev/da14c413ef663eb1ba246799e94a240f81c42488/gfx/layers/wr/WebRenderCommandBuilder.h#236-237>`_
being used switches to one specific to the new document. For this to work there are
certain assumptions that must hold, which are described in the next section.
Other things that must now be managed on a per-document basis are generally
encapsulated into the RenderRootStateManager class, and the WebRenderLayerManager
holds an `array of these
<https://searchfox.org/mozilla-central/rev/8ed8474757695cdae047150a0eaf94a5f1c96dbe/gfx/layers/wr/WebRenderLayerManager.h#242>`_.

After the Gecko display list is converted to a set of WebRender display lists
(one per document), these are sent across IPC along with any associated resources
as part of the `WebRender transaction
<https://searchfox.org/mozilla-central/rev/8ed8474757695cdae047150a0eaf94a5f1c96dbe/gfx/layers/ipc/PWebRenderBridge.ipdl#50>`_.
Conceptually, the parent side simply demultiplexes the data for different documents,
and submits the data for each document to the corresponding WebRenderAPI instance.

Limitations/Assumptions
-----------------------

One of the fundamental issues with the document splitting implementation is that
we can have stuff in the UI process that's part of the "content" renderroot (e.g.
a sidebar that appears to the left of the content area). The expectation for
front-end authors would be that this would be affected by ancestor elements that
are also in the UI process. Consider this outline of a Gecko display list:

::

  - Root display item R
    - ... stuff here (call it Q) ...
      - display item P
        - display item A
        - display item B (flagged as being in the content renderroot)
        - display item C

If item P was a filter, for example, that would normally apply to all of items
A, B, and C. This would mean either sharing the filter between the "chrome" renderroot
and the "content" renderroot, or duplicating it such that it existed in both
renderroots. The sharing is not possibly as it violates the independence of WR
documents. The duplication is technically possible, but could result in visual
glitches as the two documents would be processed and composited separately.

In order to avoid this problem, the design of document splitting explicitly assumes
that such a scenario will not happen. In particular, the only information that
gets carried across the render root boundary is the positioning offset. Any
filters, transforms that are not 2D axis-aligned, opacity, or mix blend mode
properties do NOT get carried across the render root boundary. Similarly, a
scrollframe may not contain content from multiple render roots, because that
would lead to a similar problem in APZ where it would have to update the scroll
position of scrollframes in multiple documents and they might get composited
at separate times resulting in visual glitches.

Security Concerns
-----------------

On the content side, all of the document splitting work happens in the UI process.
In other words, content processes don't generally know what document they are part
of, and don't ever split their display lists into multiple documents. Only the UI
process ever sends multiple display lists to the compositor side.

There are a number of APIs on PWebRenderBridge where a wr::RenderRoot is passed
across from the content side to the compositor side. And since PWebRenderBridge
is a unified protocol that is used by both the UI process and content processes
to communicate with the compositor, the content processes must provide *some*
value for the wr::RenderRoot. But since it doesn't (or shouldn't) be aware of
what document it's in, it must always pass wr::RenderRoot::Default.

Compositor-side code in WebRenderBridgeParent is responsible for checking that
any wr::RenderRoot values provided from a content process are in fact wr::RenderRoot::Default.
If this is not the case, it is either a programmer error or a hijacked content
process, and appropriate handling should be used. In particular, the compositor
side code should *never* blindly use the wr::RenderRoot value provided over the IPC
channel as hijacked content processes could force the compositor into leaking
information or otherwise violate the security and integrity of the browser. Instead,
the compositor is responsible for determining where the content is attached in
the display list of the UI process, and determine the appropriate document for that
content process. This information is stored in the `WebRenderBridgeParent::mRenderRoot
<https://searchfox.org/mozilla-central/rev/8ed8474757695cdae047150a0eaf94a5f1c96dbe/gfx/layers/wr/WebRenderBridgeParent.h#495>`_
field.

Implementation details
----------------------

This section describes various knots of complexity in the document splitting
implementation. That is, these pieces are thought to introduce higher-than-normal
levels of complexity into the feature, and should be handled with care.

APZ interaction
~~~~~~~~~~~~~~~

When a display list transaction is sent from the content side to the compositor,
APZ is also notified of the update, so that it can internally update its own
data structures. One of these data structures is a tree representation of the
scrollable frames on the page. With document splitting, the scrollable frames
may now be split across multiple documents. APZ needs to record which document
each scrollable frame belongs to, so that when providing the async scroll offset
to WebRender, it can send the scroll offset for a given a scrollable frame to the
correct WebRender document. As one might expect, this is stored in the `mRenderRoot
<https://searchfox.org/mozilla-central/rev/06bd14ced96f25ff1dbd5352cb985fc0fa12a64e/gfx/layers/apz/src/AsyncPanZoomController.h#916>`_
field in the AsyncPanZoomController (there is one instance of this per scrollable
frame).

Additionally, when new display list transactions and other messages are received
in WebRenderBridgeParent, APZ cannot process these updates right away. Doing so
would cause APZ to respond to user input based on the new display list, while
the WebRender internal state still corresponds to the old display list. To ensure
that APZ and WR's internal state remain in sync, APZ puts these update messages
into an `"updater queue"
<https://searchfox.org/mozilla-central/rev/06bd14ced96f25ff1dbd5352cb985fc0fa12a64e/gfx/layers/apz/src/APZUpdater.cpp#340>`_
which is processed synchronously with the WebRender scene swap. This ensures that
APZ's internal state is updated at the same time that WebRender swaps in the new
scene, and everything stays in sync.  Conceptually this is relatively simple,
until we add document splitting to the mix.

Now instead of one scene swap, we have multiple scene swaps happening, one for
each of the documents. In other words, even though WebRenderBridgeParent gets a
single "display list transaction", the display lists for the different documents
modify WR's internal state at different times. Consequently, to keep APZ in sync,
we must apply a similar "splitting" to the APZ updater queue, so that messages
pertaining to a particular document are applied synchronously with that
document's scene swap.

(As a relevant aside: there other messages that APZ receives over other IPC
channels (e.g. PAPZCTreeManager) that have ordering requirements with the
PWebRenderBridge messages, and so those also normally end up in the updater queue.
Consequently, these other messages are also now subjected to the splitting of
the updater queue.)

Again, conceptually this is relatively simple - we just need to keep a separate
queue for each document, and when an update message comes in, we decide which
document a given update message is associated with, and put the message into the
corresponding queue. The catch is that often these messages deal with a specific
element or scrollframe on the page, and so when the message is sent from the
UI process, we need to do a DOM or frame tree walk to determine which render root
that element is associated with. There are some `GetRenderRootForXXX
<https://searchfox.org/mozilla-central/rev/06bd14ced96f25ff1dbd5352cb985fc0fa12a64e/gfx/thebes/gfxUtils.h#317-322>`_
helpers in gfxUtils that assist with this task.

The other catch is that an APZ message may be associated with multiple documents.
A concrete example is if a user on a touch device does a multitouch action with
one fingers landing on different documents, which would trigger a call to
`RecvSetTargetAPZC
<https://searchfox.org/mozilla-central/rev/06bd14ced96f25ff1dbd5352cb985fc0fa12a64e/gfx/layers/ipc/APZCTreeManagerParent.cpp#76>`_
with multiple targets, each potentially belonging to a different render root.
In this case, we need to ensure that the message only gets processed after
the corresponding scene swaps for all the related documents. This is currently
implemented by having each message in the queue associated with a set of documents
rather than a single document, and only processing the message once all the
documents have done their scene swap. In the example above, this is indicated by
building the set of render roots `here
<https://searchfox.org/mozilla-central/rev/06bd14ced96f25ff1dbd5352cb985fc0fa12a64e/gfx/layers/ipc/APZCTreeManagerParent.cpp#83>`_
and passing that to the updater queue when queueing the message. This interaction
is a source of some complexity and may have latent bugs.

Deferred updates
~~~~~~~~~~~~~~~~

Bug 1547351 provided a new and tricky problem where a content process is rendering
stuff that needs go into the "default" document because it's actually an
out-of-process addon content that renders in the chrome area. Prior to this bug,
the WebRenderBridgeParent instances that corresponded to content processes
(hereafter referred to as "sub-WRBPs", in contrast to the "root WRBP" that
corresponds to the UI process) simply assumed they were in the "Content" document,
but this bug proved that this simplistic assumption does not always hold.

The solution chosen to this problem was to have the root WebRenderLayerManager
(that lives in the trusted UI process) to annotate each out-of-process subpipeline
with the render root it belongs in, and send that information over to the
root WRBP as part of the display list transaction. The sub-WRBPs know their own
pipeline ids, and therefore can find their render root by querying the root WRBP.
The catch is that sub-WRBPs may receive display list transactions before the
root WRBP receives the display list update that contains the render root mapping
information. This happens in cases like during tab switch preload, where the
user mouses over a background tab, and we pre-render it (i.e. compute and send
the display list for that tab to the compositor) so that the tab switch is faster.
In this scenario, that display list/subpipeline is not actually rendered, is not
tied in to the display list of the UI process, and therefore doesn't get associated
with a render root.

When the sub-WRBP receives a transaction in a scenario like this, it cannot
actually process it (by sending it to WebRender) because it doesn't know which
WR document it associated with. So it has to hold on to it in a "deferred update"
queue until some later point where it does find out which WR document it is
associated with, and at that point it can process the deferred update queue.

Again, conceptually this is straightforward, but the implementation produces a
bunch of complexity because it needs to handle both orderings - the case where
the sub-WRBP knows its render root, and the case where it doesn't yet. And the
root WRBP, upon receiving a new transaction, would need to notify the sub-WRBPs
of their render roots and trigger processing of the deferred updates.

Further complicating matters is Fission, because with Fission there can be
pipelines nested to arbitrary depths. This results in a tree of sub-WRBPs, with
each WRBP knowing what its direct children are, and only the root WRBP knowing
which documents its immediate children are in. So there could be a chain of
sub-WRBPs with a "missing link" (i.e. one that doesn't yet know what its children
are, because it hasn't received a display list transaction yet) and upon filling
in that missing link, all the descendant WRBPs from that point suddenly also
know which WR document they are associated with and can process their deferred
updates.

Managing all this deferred state, ensuring it is processed as soon as possible,
and clearing it out when the content side is torn down (which may happen without
it ever being rendered) is a source of complexity and may have latent bugs.

Transaction completion
~~~~~~~~~~~~~~~~~~~~~~

Transactions between the content and compositor side are throttled such that
the content side doesn't go nuts pushing over display lists to the compositor
when the compositor has a backlog of pending display lists. The way the throttling
works is that each transaction sent has a transaction id, and after the compositor
is done processing a transaction, it reports the completed transaction id back
to the content side. The content side can use this information to track how many
transactions are inflight at any given time and apply throttling as necessary.

With document splitting, a transaction sent from the content side gets split up
and sent to multiple WR documents, each of which are operating independently of
each other. If we propagate the transaction id to each of those WR documents,
then the first document to complete its work would trigger the "transaction complete"
message back to the content, which would unthrottle the next transaction. In this
scenario, other documents may still be backlogged, so the unthrottling is
undesirable.

Instead, what we want is for all documents processing a particular transaction
id to finish their word and render before we send the completion message back
to content. In fact, there's a bunch of work that falls into the same category
as this completion message - stuff that should happen after all the WR documents
are done processing their pieces of the split transaction.

The way this is managed is via a conditional in `HandleFrame
<https://searchfox.org/mozilla-central/rev/06bd14ced96f25ff1dbd5352cb985fc0fa12a64e/gfx/webrender_bindings/RenderThread.cpp#988>`_.
This code is invoked once for each document as it advances to the rendering step,
and the code in `RenderThread::IncRenderingFrameCount
<https://searchfox.org/mozilla-central/rev/06bd14ced96f25ff1dbd5352cb985fc0fa12a64e/gfx/webrender_bindings/RenderThread.cpp#552-553>`_
acts as a barrier to ensure that the call chain only gets propagated once all
the documents have done their processing work.

I'm listing this piece as a potential source of complexity for document splitting
because it seems like a fairly important piece but the relevant code that is
"buried" away in place where one might not easily stumble upon it. It's also not
clear to me that the implications of this problem and solution have been fully
explored. In particular, I assume that there are latent bugs here because other
pieces of code were assuming a certain behaviour from the pre-document-splitting
code that the post-document-splitting code may not satisfy exactly.
