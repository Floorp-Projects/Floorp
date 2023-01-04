PR_Lock
=======

Locks a specified lock object.


Syntax
------

.. code::

   #include <prlock.h>

   void PR_Lock(PRLock *lock);


Parameter
~~~~~~~~~

:ref:`PR_Lock` has one parameter:

``lock``
   A pointer to a lock object to be locked.


Description
-----------

When :ref:`PR_Lock` returns, the calling thread is "in the monitor," also
called "holding the monitor's lock." Any thread that attempts to acquire
the same lock blocks until the holder of the lock exits the monitor.
Acquiring the lock is not an interruptible operation, nor is there any
timeout mechanism.

:ref:`PR_Lock` is not reentrant. Calling it twice on the same thread
results in undefined behavior.


See Also
--------

 - :ref:`PR_Unlock`
