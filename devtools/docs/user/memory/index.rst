======
Memory
======

The Memory tool lets you take a snapshot of the current tab's memory `heap <https://en.wikipedia.org/wiki/Memory_management#HEAP>`_. It then provides a number of views of the heap that can show you which objects account for memory usage and exactly where in your code you are allocating memory.

.. raw:: html

  <iframe width="560" height="315" src="https://www.youtube.com/embed/DJLoq5E5ww0" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>
  <br/>
  <br/>


The basics
**********

- :ref:`Opening the memory tool <memory-basic-operations-opening-the-memory-tool>`
- :ref:`Taking a heap snapshot <memory-basic-operations-taking-a-heap-snapshot>`
- :ref:`Comparing two snapshots <memory-basic-operations-comparing-snapshots>`
- :ref:`Deleting snapshots <memory-basic-operations-clearing-a-snapshot>`
- :ref:`Saving and loading snapshots <memory-basic-operations-saving-and-loading-snapshots>`
- :ref:`Recording call stacks <memory-basic-operations-recording-call-stacks>`


Analyzing snapshots
*******************

The Tree map view is new in Firefox 48, and the Dominators view is new in Firefox 46.

Once you've taken a snapshot, there are three main views the Memory tool provides:


- :doc:`the Tree map view <tree_map_view/index>` shows memory usage as a `treemap <https://en.wikipedia.org/wiki/Treemapping>`_.
- :doc:`the Aggregate view <aggregate_view/index>` shows memory usage as a table of allocated types.
- :doc:`the Dominators view <dominators_view/index>` shows the "retained size" of objects: that is, the size of objects plus the size of other objects that they keep alive through references.


If you've opted to record allocation stacks for the snapshot, the Aggregate and Dominators views can show you exactly where in your code allocations are happening.

Concepts
********

- :doc:`Dominators <dominators_view/index>`


Example pages
*************

Examples used in the Memory tool documentation.

- :doc:`Monster example <monster_example/index>`
- :doc:`DOM allocation example <dom_allocation_example/index>`
