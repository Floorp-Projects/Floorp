PR_AtomicAdd
============


Syntax
------

.. code::

   #include <pratom.h>

   PRInt32 PR_AtomicAdd(
     PRInt32 *ptr,
     PRInt32 val);


Parameter
~~~~~~~~~

The function has the following parameters:

``ptr``
   A pointer to the value to increment.
``val``
   A value to be added.


Returns
~~~~~~~

The returned value is the result of the addition.


Description
-----------

Atomically add a 32 bit value.
