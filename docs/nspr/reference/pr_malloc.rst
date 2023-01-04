PR_MALLOC
=========

Allocates memory of a specified size from the heap.


Syntax
------

.. code::

   #include <prmem.h>

   void * PR_MALLOC(_bytes);


Parameter
~~~~~~~~~

``_bytes``
   Size of the requested memory block.


Returns
~~~~~~~

An untyped pointer to the allocated memory, or if the allocation attempt
fails, ``NULL``. Call ``PR_GetError()`` to retrieve the error returned
by the libc function ``malloc()``.


Description
-----------

This macro allocates memory of the requested size from the heap.
