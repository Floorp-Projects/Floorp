PR_Calloc
=========

Allocates zeroed memory from the heap for a number of objects of a given
size.


Syntax
------

.. code::

   #include <prmem.h>

   void *PR_Calloc (
      PRUint32 nelem,
      PRUint32 elsize);


Parameters
~~~~~~~~~~

``nelem``
   The number of elements of size ``elsize`` to be allocated.
``elsize``
   The size of an individual element.


Returns
~~~~~~~

An untyped pointer to the allocated memory, or if the allocation attempt
fails, ``NULL``. Call ``PR_GetError()`` to retrieve the error returned
by the libc function ``malloc()``.


Description
-----------

This function allocates memory on the heap for the specified number of
objects of the specified size. All bytes in the allocated memory are
cleared.
