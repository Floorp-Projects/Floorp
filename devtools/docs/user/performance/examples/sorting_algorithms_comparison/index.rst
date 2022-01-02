=============================
Sorting algorithms comparison
=============================

This article describes a simple example program that we use in two of the Performance guides: the guide to the :doc:`Call Tree <../../call_tree/index>` and the guide to the :doc:`Flame Chart <../../flame_chart/index>`.

This program compares the performance of three different sorting algorithms:


- bubble sort
- selection sort
- quicksort


It consists of the following functions:

.. list-table::
   :widths: 30 70
   :header-rows: 0

   * - :doc:`about:debugging <about_colon_debugging/index>`
     - Debug add-ons, content tabs, and workers running in the browser.

   * - **sortAll()**
     - Top-level function. Iteratively (200 iterations) generates a randomized array and calls ``sort()``.
   * - **sort()**
     - Calls each of ``bubbleSort()``, ``selectionSort()``, ``quickSort()`` in turn and logs the result.

   * - **bubbleSort()**
     - Implements a bubble sort, returning the sorted array.

   * - **selectionSort()**
     - Implements a selection sort, returning the sorted array.

   * - **quickSort()**
     - Implements quicksort, returning the sorted array.

   * - **swap()**
     - Helper function for ``bubbleSort()`` and ``selectionSort()``.

   * - **partition()**
     - Helper function for ``quickSort()``.


Its call graph looks like this:

.. code-block:: JavaScript

    sortAll()                 // (generate random array, then call sort) x 200
      -> sort()               // sort with each algorithm, log the result
        -> bubbleSort()
          -> swap()
        -> selectionSort()
          -> swap()
        -> quickSort()
          -> partition()

The implementations of the sorting algorithms in the program are taken from https://github.com/nzakas/computer-science-in-javascript/ and are used under the MIT license.

You can try out the example program `here <https://mdn.github.io/performance-scenarios/js-call-tree-1/index.html>`__ and clone the code `here <https://github.com/mdn/performance-scenarios>`__ (be sure to check out the gh-pages branch). You can also `download the specific profile we discuss <https://github.com/mdn/performance-scenarios/tree/gh-pages/js-call-tree-1/profile>`__ - just import it to the Performance tool if you want to follow along. Of course, you can generate your own profile, too, but the numbers will be a little different.
