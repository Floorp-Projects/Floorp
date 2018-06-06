# Clipping and Positioning in WebRender

Each non-structural WebRender display list item has
 * A `ClipId` of a positioning node
 * A `ClipId` of a clip chain
 * An item-specific rectangular clip rectangle

The positioning node determines how that item is positioned. It's assumed that the
positioning node and the item are children of the same reference frame. The clip
chain determines how that item is clipped. This should be fully independent of
how the node is positioned and items can be clipped by any `ClipChain` regardless
of the reference frame of their member clips. Finally, the item-specific
clipping rectangle is applied directly to the item and should never result in the
creation of a clip mask itself.

# The `ClipScrollTree`

The ClipScrollTree contains two sorts of elements. The first sort are nodes
that affect the positioning of display primitives and other clips. These
nodes can currently be reference frames, scroll frames, or sticky frames.
The second sort of node is a clip node, which specifies some combination of
a clipping rectangle, a collection of rounded clipping rectangles, and an
optional image mask. These nodes are created and added to the display list
during display list flattening.

# `ClipChain`s

A `ClipChain` represents some collection of `ClipId`s of clipping nodes in the
`ClipScrollTree`. The collection defines a clip mask which can be applied
to a given display item primitive. A `ClipChain` is automatically created
for every clipping node in the `ClipScrollTree` from the particular node
and every ancestor clipping node. Additionally, the API exposes functionality
to create a `ClipChain` given an arbitrary list of clipping nodes and the
`ClipId` of a parent `ClipChain`. These custom `ClipChain`s will not take
into account ancestor clipping nodes in the `ClipScrollTree` when clipping
the item.

Important information about `ClipChain`s:
 * The `ClipId`s in the list must refer to clipping nodes in the `ClipScrollTree`.
   The list should not contain `ClipId` of positioning nodes or other `ClipChain`s.
 * The `ClipId` of a clip node serves at the `ClipId` of that node's automatically
   generated `ClipChain` as well.

# The Future

In general, the clipping API is becoming more and more stable as it has become
more flexible. Some ideas for improving the API further:
 * Creating a distinction between ids that refer to `ClipScrollTree` nodes and individual
  `ClipChain`s. This would make it harder to accidentally misuse the API, but require
   `DisplayListBuilder::define_clip` to return two different types of ids.
 * Separate out the clipping nodes from the positioning nodes. Perhaps WebRender only
   needs an API where `ClipChains` are defined individually. This could potentially
   prevent unnecessary `ClipChain` creation during display list flattening.
