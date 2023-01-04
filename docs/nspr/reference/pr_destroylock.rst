PR_DestroyLock
==============

Destroys a specified lock object.


Syntax
------

.. code::

   #include <prlock.h>

   void PR_DestroyLock(PRLock *lock);


Parameter
~~~~~~~~~

:ref:`PR_DestroyLock` has one parameter:

``lock``
   A pointer to a lock object.

Caution
-------

The caller must ensure that no thread is currently in a lock-specific
function. Locks do not provide self-referential protection against
deletion.
