PR_NewThreadPrivateIndex
========================

Returns a new index for a per-thread private data table and optionally
associates a destructor with the data that will be assigned to the
index.


Syntax
------

.. code::

   #include <prthread.h>

   PRStatus PR_NewThreadPrivateIndex(
      PRUintn *newIndex,
      PRThreadPrivateDTOR destructor);


Parameters
~~~~~~~~~~

:ref:`PR_NewThreadPrivateIndex` has the following parameters:

``newIndex``
   On output, an index that is valid for all threads in the process. You
   use this index with :ref:`PR_SetThreadPrivate` and
   :ref:`PR_GetThreadPrivate`.
``destructor``
   Specifies a destructor function :ref:`PRThreadPrivateDTOR` for the
   private data associated with the index. This function can be
   specified as ``NULL``.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If the total number of indices exceeds 128, ``PR_FAILURE``.


Description
-----------

If :ref:`PR_NewThreadPrivateIndex` is successful, every thread in the same
process is capable of associating private data with the new index. Until
the data for an index is actually set, the value of the private data at
that index is ``NULL``. You pass this index to :ref:`PR_SetThreadPrivate`
and :ref:`PR_GetThreadPrivate` to set and retrieve data associated with the
index.

When you allocate the index, you may also register a destructor function
of type :ref:`PRThreadPrivateDTOR`. If a destructor function is registered
with a new index, it will be called at one of two times, as long as the
private data is not ``NULL``:

-  when replacement private data is set with :ref:`PR_SetThreadPrivate`
-  when a thread exits

The index maintains independent data values for each binding thread. A
thread can get access only to its own thread-specific data. There is no
way to deallocate a private data index once it is allocated.
