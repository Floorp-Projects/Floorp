PR_SetThreadPrivate
===================

Sets per-thread private data.


Syntax
------

.. code::

   #include <prthread.h>

   PRStatus PR_SetThreadPrivate(PRUintn index, void *priv);


Parameters
~~~~~~~~~~

:ref:`PR_SetThreadPrivate` has the following parameters:

``index``
   An index into the per-thread private data table.
``priv``
   The per-thread private data, or more likely, a pointer to the data.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If the index is invalid, ``PR_FAILURE``.


Description
-----------

If the thread already has non-``NULL`` private data associated with it,
and if the destructor function for the index is known (not ``NULL``),
NSPR calls the destructor function associated with the index before
setting the new data value. The pointer at the index is swapped with
``NULL``. If the swapped out value is not ``NULL``, the destructor
function is called. On return, the private data associated with the
index is reassigned the new private data's value, even if it is
``NULL``. The runtime provides no protection for the private data. The
destructor is called with the runtime holding no locks. Synchronization
is the client's responsibility.

The only way to eliminate thread private data at an index prior to the
thread's termination is to call :ref:`PR_SetThreadPrivate` with a ``NULL``
argument. This causes the index's destructor function to be called, and
afterwards assigns a ``NULL`` in the table. A client must not delete the
referent object of a non-``NULL`` private data without first eliminating
it from the table.
