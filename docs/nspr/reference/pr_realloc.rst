
PR_Realloc
==========

Resizes allocated memory on the heap.


Syntax
------

.. code::

   #include <prmem.h>

   void *PR_Realloc (
      void *ptr,
      PRUint32 size);


Parameters
~~~~~~~~~~

``ptr``
   A pointer to the existing memory block being resized.
``size``
   The size of the new memory block.


Returns
~~~~~~~

An untyped pointer to the allocated memory, or if the allocation attempt
fails, ``NULL``. Call ``PR_GetError()`` to retrieve the error returned
by the libc function ``realloc()``.


Description
~~~~~~~~~~~

This function attempts to enlarge or shrink the memory block addressed
by ptr to a new size. The contents of the specified memory remains the
same up to the smaller of its old size and new size, although the new
memory block's address can be different from the original address.
