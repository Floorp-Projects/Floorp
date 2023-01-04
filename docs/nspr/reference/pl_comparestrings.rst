PL_CompareStrings
=================

Compares two character strings.


Syntax
------

.. code::

   #include <plhash.h>

   PRIntn PL_CompareStrings(
     const void *v1,
     const void *v2);


Description
-----------

:ref:`PL_CompareStrings` compares ``v1`` and ``v2`` as character strings
using ``strcmp``. If the two strings are equal, it returns 1. If the two
strings are not equal, it returns 0.

:ref:`PL_CompareStrings` can be used as the comparator function for
string-valued key or entry value.
