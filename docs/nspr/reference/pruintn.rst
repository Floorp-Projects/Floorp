PRUintn
=======

This (unsigned) type is one of the most appropriate for automatic
variables. It is guaranteed to be at least 16 bits, though various
architectures may define it to be wider (for example, 32 or even 64
bits). This types is never valid for fields of a structure.


Syntax
------

.. code::

   #include <prtypes.h>

   typedef unsigned int PRUintn;
