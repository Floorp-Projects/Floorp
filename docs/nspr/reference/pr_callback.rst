PR_CALLBACKimplementation
=========================

Used to define pointers to functions that will be implemented by the
client but called from a (different) shared library.


Syntax
------

.. code::

   #include <prtypes.h>type PR_CALLBACKimplementation


Description
-----------

Functions that are implemented in an application (or shared library)
that are intended to be called from another shared library (such as
NSPR) must be declared with the ``PR_CALLBACK`` attribute. Normally such
functions are passed by reference (pointer to function). The
``PR_CALLBACK`` attribute is included as part of the function's
definition between its return value type and the function's name.
