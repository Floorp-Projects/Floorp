PR_DELETE
=========


Allocates memory of a specified size from the heap.


Syntax
------

.. code::

   #include <prmem.h>

   void PR_DELETE(_ptr);


Parameter
~~~~~~~~~

``_ptr``
   The address of memory to be returned to the heap. Must be an lvalue
   (an expression that can appear on the left side of an assignment
   statement).


Returns
~~~~~~~

Nothing.


Description
-----------

This macro returns allocated memory to the heap from the specified
location and sets ``_ptr`` to ``NULL``.
