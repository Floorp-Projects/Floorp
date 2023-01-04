PR_AtomicSet
============

Atomically sets a 32-bit value and return its previous contents.


Syntax
------

.. code::

   #include <pratom.h>

   PRInt32 PR_AtomicSet(
     PRInt32 *val,
     PRInt32 newval);


Parameters
~~~~~~~~~~

The function has the following parameter:

``val``
   A pointer to the value to be set.
``newval``
   The new value to assign to the ``val`` parameter.


Returns
~~~~~~~

The function returns the prior value of the referenced variable.


Description
-----------

:ref:`PR_AtomicSet` first reads the value of var, then updates it with the
supplied value. The returned value is the value that was read\ *before*
memory was updated. The memory modification is unconditional--that is,
it isn't a test and set operation.
