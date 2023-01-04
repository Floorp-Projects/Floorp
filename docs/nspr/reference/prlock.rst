PRLock
======

A mutual exclusion lock.


Syntax
------

.. code::

   #include <prlock.h>

   typedef struct PRLock PRLock;


Description
-----------

NSPR represents a lock as an opaque entity to clients of the functions
described in `"Locks" <en/NSPR_API_Reference/Locks>`__. Functions that
operate on locks do not have timeouts and are not interruptible.
