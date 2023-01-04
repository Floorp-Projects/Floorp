PR_EnterMonitor
===============

Enters the lock associated with a specified monitor.


Syntax
------

.. code::

   #include <prmon.h>

   void PR_EnterMonitor(PRMonitor *mon);


Parameter
~~~~~~~~~

The function has the following parameter:

``mon``
   A reference to an existing structure of type :ref:`PRMonitor`.


Description
-----------

When the calling thread returns, it will have acquired the monitor's
lock. Attempts to acquire the lock for a monitor that is held by some
other thread will result in the caller blocking. The operation is
neither timed nor interruptible.

If the monitor's entry count is greater than zero and the calling thread
is recognized as the holder of the lock, :ref:`PR_EnterMonitor` increments
the entry count by one and returns. If the entry count is greater than
zero and the calling thread is not recognized as the holder of the lock,
the thread is blocked until the entry count reaches zero. When the entry
count reaches zero (or if it is already zero), the entry count is
incremented by one and the calling thread is recorded as the lock's
holder.
