PR_FREEIF
=========

Conditionally frees allocated memory.


Syntax
------

.. code::

   #include <prmem.h>

   void PR_FREEIF(_ptr);


Parameter
~~~~~~~~~

``_ptr``
   The address of memory to be returned to the heap.


Returns
~~~~~~~

Nothing.


Description
-----------

This macro returns memory to the heap when ``_ptr`` is not ``NULL``. If
``_ptr`` is ``NULL``, the macro has no effect.
