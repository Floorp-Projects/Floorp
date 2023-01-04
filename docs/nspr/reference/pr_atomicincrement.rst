PR_AtomicIncrement
==================

Atomically increments a 32-bit value.


Syntax
------

.. code::

   #include <pratom.h>

   PRInt32 PR_AtomicIncrement(PRInt32 *val);


Parameter
~~~~~~~~~

The function has the following parameter:

``val``
   A pointer to the value to increment.


Returns
~~~~~~~

The function returns the incremented value (i.e., the result).


Description
-----------

The referenced variable is incremented by one. The result of the
function is the value of the memory after the operation. The writing of
the memory is unconditional.
