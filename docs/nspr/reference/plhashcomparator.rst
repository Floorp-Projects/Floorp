PLHashComparator
================


Syntax
------

.. code::

   #include <plhash.h>

   typedef PRIntn (PR_CALLBACK *PLHashComparator)(
     const void *v1,
     const void *v2);


Description
-----------

:ref:`PLHashComparator` is a function type that compares two values of an
unspecified type. It returns a nonzero value if the two values are
equal, and 0 if the two values are not equal. :ref:`PLHashComparator`
defines the meaning of equality for the unspecified type.

For convenience, two comparator functions are provided.
:ref:`PL_CompareStrings` compare two character strings using ``strcmp``.
:ref:`PL_CompareValues` compares the values of the arguments v1 and v2
numerically.


Remark
------

The return value of :ref:`PLHashComparator` functions should be of type
:ref:`PRBool`.


See Also
--------

:ref:`PL_CompareStrings`, :ref:`PL_CompareValues`
