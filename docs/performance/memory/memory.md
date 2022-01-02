# Memory Tools

The Memory tool lets you take a snapshot of the current tab's memory
[heap](https://en.wikipedia.org/wiki/Memory_management#HEAP). 
It then provides a number of views of the heap that can
show you which objects account for memory usage and exactly where in
your code you are allocating memory.

<iframe width="595" height="325" src="https://www.youtube.com/embed/DJLoq5E5ww0" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture"></iframe>

------------------------------------------------------------------------

### The basics
-   Opening [the memory
    tool](basic_operations.html#opening-the-memory-tool)
-   [Taking a heap
    snapshot](basic_operations.html#saving-and-loading-snapshots)
-   [Comparing two
    snapshots](basic_operations.html#comparing-snapshots)
-   [Deleting
    snapshots](basic_operations.html#clearing-a-snapshot)
-   [Saving and loading
    snapshots](basic_operations.html#saving-and-loading-snapshots)
-   [Recording call
    stacks](basic_operations.html#recording-call-stacks)

------------------------------------------------------------------------

### Analyzing snapshots

The Tree map view is new in Firefox 48, and the Dominators view is new
in Firefox 46.

Once you've taken a snapshot, there are three main views the Memory
tool provides:

-   [the Tree map view](tree_map_view.md) shows
    memory usage as a
    [treemap](https://en.wikipedia.org/wiki/Treemapping).
-   [the Aggregate view](aggregate_view.md) shows
    memory usage as a table of allocated types.
-   [the Dominators view](dominators_view.md)
    shows the "retained size" of objects: that is, the size of objects
    plus the size of other objects that they keep alive through
    references.

If you've opted to record allocation stacks for the snapshot, the
Aggregate and Dominators views can show you exactly where in your code
allocations are happening.

------------------------------------------------------------------------

### Concepts

-   What are [Dominators](dominators.md)?

------------------------------------------------------------------------

### Example pages

Examples used in the Memory tool documentation.

-   The [Monster example](monster_example.md)
-   The [DOM allocation example](DOM_allocation_example.md)
