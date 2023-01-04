PR_NEWZAP
=========

Allocates and clears memory from the heap for an instance of a given
type.


Syntax
------

.. code::

   #include <prmem.h>

   _type * PR_NEWZAP(_struct);


Parameter
~~~~~~~~~

``_struct``
   The name of a type.


Returns
~~~~~~~

An pointer to a buffer sized to contain the type ``_struct``, or if the
allocation attempt fails, ``NULL``. The bytes in the buffer are all
initialized to 0. Call ``PR_GetError()`` to retrieve the error returned
by the libc function.


Description
-----------

This macro allocates an instance of the specified type from the heap and
sets the content of that memory to zero.
