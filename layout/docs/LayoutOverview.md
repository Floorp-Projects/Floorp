# Layout Overview

Much of the layout code deals with operations on the frame tree (or
rendering tree). In the frame tree, each node represents a rectangle
(or, for SVG, other shapes). The frame tree has a shape similar to the
content tree, since many content nodes have one corresponding frame,
though it differs in a few ways, since some content nodes have more than
one frame or don\'t have any frames at all. When elements are
display:none in CSS or undisplayed for certain other reasons, they
won\'t have any frames. When elements are broken across lines or pages,
they have multiple frames; elements may also have multiple frames when
multiple frames nested inside each other are needed to display a single
element (for example, a table, a table cell, or many types of form
controls).

Each node in the frame tree is an instance of a class derived from
`nsIFrame`. As with the content tree, there is a substantial type
hierarchy, but the type hierarchy is very different: it includes types
like text frames, blocks and inlines, the various parts of tables, flex
and grid containers, and the various types of HTML form controls.

Frames are allocated within an arena owned by the PresShell. Each frame
is owned by its parent; frames are not reference counted, and code must
not hold on to pointers to frames. To mitigate potential security bugs
when pointers to destroyed frames, we use [frame
poisoning](http://robert.ocallahan.org/2010/10/mitigating-dangling-pointer-bugs-using_15.html),
which takes two parts. When a frame is destroyed other than at the end
of life of the presentation, we fill its memory with a pattern
consisting of a repeated pointer to inaccessible memory, and then put
the memory on a per-frame-class freelist. This means that if code
accesses the memory through a dangling pointer, it will either crash
quickly by dereferencing the poison pattern or it will find a valid
frame.

Like the content tree, frames must be accessed only from the UI thread.

The frame tree should not store any important data, i.e. any data that
cannot be recomputed on-the-fly. While the frame tree does usually
persist while a page is being displayed, frames are often destroyed and
recreated in response to certain style changes, and in the future we may
do the same to reduce memory use for pages that are currently inactive.
There were a number of cases where this rule was violated in the past
and we stored important data in the frame tree; however, most (though
not quite all) such cases are now fixed.

The rectangle represented by the frame is what CSS calls the element\'s
border box. This is the outside edge of the border (or the inside edge
of the margin). The margin lives outside the border; and the padding
lives inside the border. In addition to nsIFrame::GetRect, we also have
the APIs nsIFrame::GetPaddingRect to get the padding box (the outside
edge of the padding, or inside edge of the border) and
nsIFrame::GetContentRect to get the content box (the outside edge of the
content, or inside edge of the padding). These APIs may produce out of
date results when reflow is needed (or has not yet occurred).

In addition to tracking a rectangle, frames also track two overflow
areas: ink overflow and scrollable overflow. These overflow areas
represent the union of the area needed by the frame and by all its
descendants. The ink overflow is used for painting-related
optimizations: it is a rectangle covering all of the area that might be
painted when the frame and all of its descendants paint. The scrollable
overflow represents the area that the user should be able to scroll to
to see the frame and all of its descendants. In some cases differences
between the frame\'s rect and its overflow happen because of descendants
that stick out of the frame; in other cases they occur because of some
characteristic of the frame itself. The two overflow areas are similar,
but there are differences: for example, margins are part of scrollable
overflow but not ink overflow, whereas text-shadows are part of ink
overflow but not scrollable overflow.

When frames are broken across lines, columns, or pages, we create
multiple frames representing the multiple rectangles of the element. The
first one is the primary frame, and the rest are its continuations
(which are more likely to be destroyed and recreated during reflow).
These frames are linked together as continuations: they have a
doubly-linked list that can be used to traverse the continuations using
nsIFrame::GetPrevContinuation and nsIFrame::GetNextContinuation.
(Currently continuations always have the same style data, though we may
at some point want to break that invariant.)

Continuations are sometimes siblings of each other (i.e.
nsIFrame::GetNextContinuation and nsIFrame::GetNextSibling might return
the same frame), and sometimes not. For example, if a paragraph contains
a span which contains a link, and the link is split across lines, then
the continuations of the span are siblings (since they are both children
of the paragraph), but the continuations of the link are not siblings
(since each continuation of the link is descended from a different
continuation of the span). Traversing the entire frame tree does **not**
require explicit traversal of any frames\' continuations-list, since all
of the continuations are descendants of the element containing the
break.

We also use continuations for cases (most importantly, bidi reordering,
where left-to-right text and right-to-left text need to be separated
into different continuations since they may not form a contiguous
rectangle) where the continuations should not be rewrapped during
reflow: we call these continuations fixed rather than fluid.
nsIFrame::GetNextInFlow and nsIFrame::GetPrevInFlow traverse only the
fluid continuations and do not cross fixed continuation boundaries.

If an inline frame has non-inline children, then we split the original
inline frame into parts. The original inline\'s children are distributed
into these parts like so: The children of the original inline are
grouped into runs of inline and non-inline, and runs of inline get an
inline parent, while runs of non-inline get an anonymous block parent.
We call this \'ib-splitting\' or \'block-inside-inline splitting\'. This
splitting proceeds recursively up the frame tree until all non-inlines
inside inlines are ancestors of a block frame with anonymous block
wrappers in between. This splitting maintains the relative order between
these child frames, and the relationship between the parts of a split
inline is maintained using an ib-sibling chain. It is important to note
that any wrappers created during frame construction (such as for tables)
might not be included in the ib-sibling chain depending on when this
wrapper creation takes place.

TODO: nsBox craziness from
<https://bugzilla.mozilla.org/show_bug.cgi?id=524925#c64>

TODO: link to documentation of block and inline layout

TODO: link to documentation of scrollframes

TODO: link to documentation of XUL frame classes

Code (note that most files in base and generic have useful one line
descriptions at the top that show up in DXR):

-   [layout/base/](http://dxr.mozilla.org/mozilla-central/source/layout/base/)
    contains objects that coordinate everything and a bunch of other
    miscellaneous things
-   [layout/generic/](http://dxr.mozilla.org/mozilla-central/source/layout/generic/)
    contains the basic frame classes as well as support code for their
    reflow methods (ReflowInput, ReflowOutput)
-   [layout/forms/](http://dxr.mozilla.org/mozilla-central/source/layout/forms/)
    contains frame classes for HTML form controls
-   [layout/tables/](http://dxr.mozilla.org/mozilla-central/source/layout/tables/)
    contains frame classes for CSS/HTML tables
-   [layout/mathml/](http://dxr.mozilla.org/mozilla-central/source/layout/mathml/)
    contains frame classes for MathML
-   [layout/svg/](http://dxr.mozilla.org/mozilla-central/source/layout/svg/)
    contains frame classes for SVG
-   [layout/xul/](http://dxr.mozilla.org/mozilla-central/source/layout/xul/)
    contains frame classes for the XUL box model and for various XUL
    widgets

Bugzilla:

-   All of the components whose names begin with \"Layout\" in the
    \"Core\" product

Further documentation:

-   Talk: [Introduction to graphics/layout
    architecture](https://air.mozilla.org/introduction-to-graphics-layout-architecture/)
    (Robert O\'Callahan, 2014-04-18)
-   Talk: [Layout and
    Styles](https://air.mozilla.org/bz-layout-and-styles/) (Boris
    Zbarsky, 2014-10-14)

## Frame Construction

Frame construction is the process of creating frames. This is done when
styles change in ways that require frames to be created or recreated or
when nodes are inserted into the document. The content tree and the
frame tree don\'t have quite the same shape, and the frame construction
process does some of the work of creating the right shape for the frame
tree. It handles the aspects of creating the right shape that don\'t
depend on layout information. So for example, frame construction handles
the work needed to implement [table anonymous
objects](http://www.w3.org/TR/CSS21/tables.html#anonymous-boxes) but
does not handle frames that need to be created when an element is broken
across lines or pages.

The basic unit of frame construction is a run of contiguous children of
a single parent element. When asked to construct frames for such a run
of children, the frame constructor first determines, based on the
siblings and parent of the nodes involved, where in the frame tree the
new frames should be inserted. Then the frame constructor walks through
the list of content nodes involved and for each one creates a temporary
data structure called a **frame construction item**. The frame
construction item encapsulates various information needed to create the
frames for the content node: its style data, some metadata about how one
would create a frame for this node based on its namespace, tag name, and
styles, and some data about what sort of frame will be created. This
list of frame construction items is then analyzed to see whether
constructing frames based on it and inserting them at the chosen
insertion point will produce a valid frame tree. If it will not, the
frame constructor either fixes up the list of frame construction items
so that the resulting frame tree would be valid or throws away the list
of frame construction items and requests the destruction and re-creation
of the frame for the parent element so that it has a chance to create a
list of frame construction items that it `<em>`{=html}can`</em>`{=html}
fix up.

Once the frame constructor has a list of frame construction items and an
insertion point that would lead to a valid frame tree, it goes ahead and
creates frames based on those items. Creation of a non-leaf frame
recursively attempts to create frames for the children of that frame\'s
element, so in effect frames are created in a depth-first traversal of
the content tree.

The vast majority of the code in the frame constructor, therefore, falls
into one of these categories:

-   Code to determine the correct insertion point in the frame tree for
    new frames.
-   Code to create, for a given content node, frame construction items.
    This involves some searches through static data tables for metadata
    about the frame to be created.
-   Code to analyze the list of frame construction items.
-   Code to fix up the list of frame construction items.
-   Code to create frames from frame construction items.

Code:
[layout/base/nsCSSFrameConstructor.h](http://dxr.mozilla.org/mozilla-central/source/layout/base/nsCSSFrameConstructor.h)
and
[layout/base/nsCSSFrameConstructor.cpp](http://dxr.mozilla.org/mozilla-central/source/layout/base/nsCSSFrameConstructor.cpp)

## Physical Sizes vs. Logical Sizes

TODO: Discuss inline-size (typically width) and block size (typically
height), writing modes, and the various logical vs. physical size/rect
types.

## Reflow

Reflow is the process of computing the positions and sizes of frames.
(After all, frames represent rectangles, and at some point we need to
figure out exactly \*what\* rectangle.) Reflow is done recursively, with
each frame\'s Reflow method calling the Reflow methods on that frame\'s
descendants.

In many cases, the correct results are defined by CSS specifications
(particularly [CSS 2.1](http://www.w3.org/TR/CSS21/visudet.html)). In
some cases, the details are not defined by CSS, though in some (but not
all) of those cases we are constrained by Web compatibility. When the
details are defined by CSS, however, the code to compute the layout is
generally structured somewhat differently from the way it is described
in the CSS specifications, since the CSS specifications are generally
written in terms of constraints, whereas our layout code consists of
algorithms optimized for incremental recomputation.

The reflow generally starts from the root of the frame tree, though some
other types of frame can act as \"reflow roots\" and start a reflow from
them (nsTextControlFrame is one example; see the
[NS\_FRAME\_REFLOW\_ROOT](https://searchfox.org/mozilla-central/search?q=symbol:E_%3CT_nsFrameState%3E_NS_FRAME_REFLOW_ROOT&redirect=true)
frame state bit). Reflow roots must obey the invariant that a change
inside one of their descendants never changes their rect or overflow
areas (though currently scrollbars are reflow roots but don\'t quite
obey this invariant).

In many cases, we want to reflow a part of the frame tree, and we want
this reflow to be efficient. For example, when content is added or
removed from the document tree or when styles change, we want the amount
of work we need to redo to be proportional to the amount of content. We
also want to efficiently handle a series of changes to the same content.

To do this, we maintain two bits on frames:
[NS\_FRAME\_IS\_DIRTY](https://searchfox.org/mozilla-central/search?q=symbol:E_%3CT_nsFrameState%3E_NS_FRAME_IS_DIRTY&redirect=true)
indicates that a frame and all of its descendants require reflow.
[NS\_FRAME\_HAS\_DIRTY\_CHILDREN](https://searchfox.org/mozilla-central/search?q=symbol:E_%3CT_nsFrameState%3E_NS_FRAME_HAS_DIRTY_CHILDREN&redirect=true)
indicates that a frame has a descendant that is dirty or has had a
descendant removed (i.e., that it has a child that has
NS\_FRAME\_IS\_DIRTY or NS\_FRAME\_HAS\_DIRTY\_CHILDREN or it had a
child removed). These bits allow coalescing of multiple updates; this
coalescing is done in PresShell, which tracks the set of reflow roots
that require reflow. The bits are set during calls to
[PresShell::FrameNeedsReflow](https://searchfox.org/mozilla-central/search?q=PresShell%3A%3AFrameNeedsReflow&path=)
and are cleared during reflow.

The layout algorithms used by many of the frame classes are those
specified in CSS, which are based on the traditional document formatting
model, where widths are input and heights are output.

In some cases, however, widths need to be determined based on the
content. This depends on two *intrinsic widths*: the minimum intrinsic
width (see
[nsIFrame::GetMinISize](https://searchfox.org/mozilla-central/search?q=nsIFrame%3A%3AGetMinISize&path=))
and the preferred intrinsic width (see
[nsIFrame::GetPrefISize](https://searchfox.org/mozilla-central/search?q=nsIFrame%3A%3AGetPrefISize&path=)).
The concept of what these widths represent is best explained by
describing what they are on a paragraph containing only text: in such a
paragraph the minimum intrinsic width is the width of the longest word,
and the preferred intrinsic width is the width of the entire paragraph
laid out on one line.

Intrinsic widths are invalidated separately from the dirty bits
described above. When a caller informs the pres shell that a frame needs
reflow (PresShell::FrameNeedsReflow), it passes one of three options:

-   eResize indicates that no intrinsic widths are dirty
-   eTreeChange indicates that intrinsic widths on it and its ancestors
    are dirty (which happens, for example, if new children are added to
    it)
-   eStyleChange indicates that intrinsic widths on it, its ancestors,
    and its descendants are dirty (for example, if the font-size
    changes)

Reflow is the area where the XUL frame classes (those that inherit from
nsBoxFrame or nsLeafBoxFrame) are most different from the rest. Instead
of using nsIFrame::Reflow, they do their layout computations using
intrinsic size methods called GetMinSize, GetPrefSize, and GetMaxSize
(which report intrinsic sizes in two dimensions) and a final layout
method called Layout. In many cases these methods defer some of the
computation to a separate object called a layout manager.

When an individual frame\'s Reflow method is called, most of the input
is provided on an object called ReflowInput and the output is filled in
to an object called ReflowOutput. After reflow, the caller (usually the
parent) is responsible for setting the frame\'s size based on the
metrics reported. (This can make some computations during reflow
difficult, since the new size is found in either the reflow state or the
metrics, but the frame\'s size is still the old size. However, it\'s
useful for invalidating the correct areas that need to be repainted.)

One major difference worth noting is that in XUL layout, the size of the
child is set prior to its parent calling its Layout method. (Once
invalidation uses display lists and is no longer tangled up in Reflow,
it may be worth switching non-XUL layout to work this way as well.)

## Painting

TODO: display lists (and event handling)

TODO: layers

## Pagination

Pagination (also known as fragmentation) is a concept used in printing,
print-preview, and multicolumn layout.

### Continuations in the Frame Tree

To render a DOM node, represented as `nsIContent` object, Gecko creates
zero or more frames (`nsIFrame` objects). Each frame represents a
rectangular area usually corresponding to the node\'s CSS box as
described by the CSS specs. Simple elements are often representable with
exactly one frame, but sometimes an element needs to be represented with
more than one frame. For example, text breaking across lines:

      xxxxxx AAAA
      AAA xxxxxxx

The A element is a single DOM node but obviously a single rectangular
frame isn\'t going to represent its layout precisely.

Similarly, consider text breaking across pages:

      | BBBBBBBBBB |
      | BBBBBBBBBB |
      +------------+

      +------------+
      | BBBBBBBBBB |
      | BBBBBBBBBB |
      |            |

Again, a single rectangular frame cannot represent the layout of the
node. Columns are similar.

Another case where a single DOM node is represented by multiple frames
is when a text node contains bidirectional text (e.g. both Hebrew and
English text). In this case, the text node and its inline ancestors are
split so that each frame contains only unidirectional text.

The first frame for an element is called the **primary frame**. The
other frames are called **continuation frames**. Primary frames are
created by `nsCSSFrameConstructor` in response to content insertion
notifications. Continuation frames are created during bidi resolution,
and during reflow, when reflow detects that a content element cannot be
fully laid out within the constraints assigned (e.g., when inline text
will not fit within a particular width constraint, or when a block
cannot be laid out within a particular height constraint).

Continuation frames created during reflow are called \"fluid\"
continuations (or \"in-flows\"). Other continuation frames (currently,
those created during bidi resolution), are, in contrast, \"non-fluid\".
The `NS_FRAME_IS_FLUID_CONTINUATION` state bit indicates whether a
continuation frame is fluid or not.

The frames for an element are put in a doubly-linked list. The links are
accessible via `nsIFrame::GetNextContinuation` and
`nsIFrame::GetPrevContinuation`. If only fluid continuations are to be
accessed, `nsIFrame::GetNextInFlow` and `nsIFrame::GetPrevInFlow` are
used instead.

The following diagram shows the relationship between the original frame
tree considering just primary frames, and a possible layout with
breaking and continuations:

    Original frame tree       Frame tree with A broken into three parts
        Root                      Root
         |                      /  |  \
         A                     A1  A2  A3
        / \                   / |  |    |
       B   C                 B  C1 C2   C3
       |  /|\                |  |  | \   |
       D E F G               D  E  F G1  G2

Certain kinds of frames create multiple child frames for the same
content element:

-   `nsPageSequenceFrame` creates multiple page children, each one
    associated with the entire document, separated by page breaks
-   `nsColumnSetFrame` creates multiple block children, each one
    associated with the column element, separated by column breaks
-   `nsBlockFrame` creates multiple inline children, each one associated
    with the same inline element, separated by line breaks, or by
    changes in text direction
-   `nsTableColFrame` creates non-fluid continuations for itself if it
    has span=\"N\" and N \> 1
-   If a block frame is a multi-column container and has
    `column-span:all` children, it creates multiple `nsColumnSetFrame`
    children, which are linked together as non-fluid continuations.
    Similarly, if a block frame is within a multi-column formatting
    context and has `column-span:all` children, it is chopped into
    several flows, which are linked together as non-fluid continuations
    as well. See documentation and example frame trees in
    [`nsCSSFrameConstructor::ConstructBlock()`](https://searchfox.org/mozilla-central/rev/d24696b5abaf9fb75f7985952eab50d5f4ed52ac/layout/base/nsCSSFrameConstructor.cpp#10431).

#### Overflow Container Continuations

Sometimes the content of a frame needs to break across pages even though
the frame itself is complete. This usually happens if an element with
fixed height has overflow that doesn\'t fit on one page. In this case,
the completed frame is \"overflow incomplete\", and special
continuations are created to hold its overflow. These continuations are
called \"overflow containers\". They are invisible, and are kept on a
special list in their parent. See documentation in
[nsContainerFrame.h](https://searchfox.org/mozilla-central/source/layout/generic/nsContainerFrame.h)
and example trees in [bug 379349 comment
3](https://bugzilla.mozilla.org/show_bug.cgi?id=379349#c3).

This infrastructure was extended in [bug
154892](https://bugzilla.mozilla.org/show_bug.cgi?id=154892) to also
manage continuations for absolutely-positioned frames.

#### Relationship of continuations to frame tree structure

It is worth emphasizing two points about the relationship of the
prev-continuation / next-continuation linkage to the existing frame tree
structure.

First, if you want to traverse the frame tree or a subtree thereof to
examine all the frames once, you do `<em>`{=html}not`</em>`{=html} want
to traverse next-continuation links. All continuations are reachable by
traversing the `GetNextSibling` links from the result of `GetFirstChild`
for all child lists.

Second, the following property holds:

-   Consider two frames F1 and F2 where F1\'s next-continuation is F2
    and their respective parent frames are P1 and P2. Then either P1\'s
    next continuation is P2, or P1 == P2, because P is responsible for
    breaking F1 and F2.

In other words, continuations are sometimes siblings of each other, and
sometimes not. If their parent content was broken at the same point,
then they are not siblings, since they are children of different
continuations of the parent. So in the frame tree for the markup

` <p>This is <b><i>some <br/>text</i></b>.</p>`

the two continuations for the `b` element are siblings (unless the line
break is also a page break), but the two continuations for the `i`
element are not.

There is an exception to that property when F1 is a first-in-flow float
placeholder. In that case F2\'s parent will be the next-in-flow of F1\'s
containing block.

### Reflow statuses

The aStatus argument of Reflow reflects that. `IsComplete()` means that
we reflowed all the content and no more next-in-flows are needed. At
that point there may still be next in flows, but the parent will delete
them. `IsIncomplete()` means \"some content did not fit in this frame\".
`IsOverflowIncomplete()` means that the frame is itself complete, but
some of its content didn\'t fit: this triggers the creation of overflow
containers for the frame\'s continuations. `IsIncomplete()` and
`NextInFlowNeedsReflow()` means \"some content did not fit in this frame
AND it must be reflowed\". These values are defined and documented in
[nsIFrame.h](https://searchfox.org/mozilla-central/source/layout/generic/nsIFrame.h)
(search for \"Reflow status\").

### Dynamic Reflow Considerations

When we reflow a frame F with fluid continuations, two things can
happen:

-   Some child frames do not fit in the passed-in width or height
    constraint. These frames must be \"pushed\" to F\'s next-in-flow. If
    F has no next-in-flow, we must create one under F\'s parent\'s
    next-in-flow \-\-- or if F\'s parent is managing the breaking of F,
    then we create F\'s next in flow directly under F\'s parent. If F is
    a block, it pushes overflowing child frames to its \"overflow\"
    child list and forces F\'s next in flow to be reflowed. When we
    reflow a block, we pull the child frames from the prev-in-flow\'s
    overflow list into the current frame.
-   All child frames fit in the passed-in width or height constraint.
    Then child frames must be \"pulled\" from F\'s next-in-flow to fill
    in the available space. If F\'s next-in-flow becomes empty, we may
    be able to delete it.

In both of these situations we might end up with a frame F containing
two child frames, one of which is a continuation of the other. This is
incorrect. We might also create holes, where there are frames P1 P2 and
P3, P1 has child F1 and P3 has child F2, but P2 has no F child.

A strategy for avoiding these issues is this: When pulling a frame F2
from parent P2 to prev-in-flow P1, if F2 is a breakable container, then:

-   If F2 has no prev-in-flow F1 in P1, then create a new primary frame
    F1 in P1 for F2\'s content, with F2 as its next-in-flow.
-   Pull children from F2 to F1 until F2 is empty or we run out of
    space. If F2 goes empty, pull from the next non-empty next-in-flow.
    Empty continuations with no next-in-flows can be deleted.

When pushing a frame F1 from parent P1 to P2, where F1 has a
next-in-flow F2 (which must be a child of P2):

-   Merge F2 into F1 by moving all F2\'s children into F1, then deleting
    F2

For inline frames F, we have our own custom strategy that coalesces
adjacent inline frames. This need not change.

We do need to implement this strategy when F is a normal in-flow block,
a floating block, and eventually an absolutely positioned block.
