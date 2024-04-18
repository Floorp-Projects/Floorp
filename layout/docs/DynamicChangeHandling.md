# Dynamic change handling along the rendering pipeline

The ability to make changes to the DOM from script is a major feature of
the Web platform. Web authors rely on the concept (though there are a
few exceptions, such as animations) that changing the DOM from script
leads to the same rendering that would have resulted from starting from
that DOM tree. They also rely on the performance characteristics of
these changes: small changes to the DOM that have small effects should
have proportionally small processing time. This means that Gecko needs
to efficiently propagate changes from the content tree to style, the
frame tree, the geometry of the frame tree, and the screen.

For many types of changes, however, there is substantial overhead to
processing a change, no matter how small. For example, reflow must
propagate from the top of the frame tree down to the frames that are
dirty, no matter how small the change. One very common way around this
is to batch up changes. We batch up changes in lots of ways, for
example:

-   The content sink adds multiple nodes to the DOM tree before
    notifying listeners that they\'ve been added. This allows notifying
    once about an ancestor rather than for each of its descendants, or
    notifying about a group of descendants all at once, which speeds up
    the processing of those notifications.
-   We batch up nodes that require style reresolution (recomputation of
    selector matching and processing the resulting style changes). This
    batching is tree based, so it not only merges multiple notifications
    on the same element, but also merges a notification on an ancestor
    with a notification on its descendant (since *some* of these
    notifications imply that style reresolution is required on all
    descendants).
-   We wait to reconstruct frames that require reconstruction (after
    destroying frames eagerly). This, like the tree-based style
    reresolution batching, avoids duplication both for same-element
    notifications and ancestor-descendant notifications, even though it
    doesn\'t actually do any tree-based caching.
-   We postpone doing reflows until needed. As for style reresolution,
    this maintains tree-based dirty bits (see the description of
    NS\_FRAME\_IS\_DIRTY and NS\_FRAME\_HAS\_DIRTY\_CHILDREN under
    Reflow).
-   We allow the OS to queue up multiple invalidates before repainting
    (though we will likely switch to controlling that ourselves). This
    leads to a single repaint of some set of pixels where there might
    otherwise have been multiple (though it may also lead to more pixels
    being repainted if multiple rectangles are merged to a single one).

Having changes buffered up means, however, that various pieces of
information (layout, style, etc.) may not be up-to-date. Some things
require up-to-date information: for example, we don\'t want to expose
the details of our buffering to Web page script since the programming
model of Web page script assumes that DOM changes take effect
\"immediately\", i.e., that the script shouldn\'t be able to detect any
buffering. Many Web pages depend on this.

We therefore have ways to flush these different sorts of buffers. There
are methods called FlushPendingNotifications on nsIDocument and
nsIPresShell, that take an argument of what things to flush:

-   Flush\_Content: create all the content nodes from data buffered in
    the parser
-   Flush\_ContentAndNotify: the above, plus notify document observers
    about the creation of all nodes created so far
-   Flush\_Style: the above, plus make sure style data are up-to-date
-   Flush\_Frames: the above, plus make sure all frame construction has
    happened (currently the same as Flush\_Style)
-   Flush\_InterruptibleLayout: the above, plus perform layout (Reflow),
    but allow interrupting layout if it takes too long
-   Flush\_Layout: the above, plus ensure layout (Reflow) runs to
    completion
-   Flush\_Display (should never be used): the above, plus ensure
    repainting happens

The major way that notifications of changes propagate from the content
code to layout and other areas of code is through the
nsIDocumentObserver and nsIMutationObserver interfaces. Classes can
implement this interface to listen to notifications of changes for an
entire document or for a subtree of the content tree.

WRITE ME: \... layout document observer implementations

TODO: how style system optimizes away rerunning selector matching

TODO: style changes and nsChangeHint
