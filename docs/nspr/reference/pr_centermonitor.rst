PR_CEnterMonitor
================

Enters the lock associated with a cached monitor.


Syntax
------

.. code::

   #include <prcmon.h>

   PRMonitor* PR_CEnterMonitor(void *address);


Parameter
~~~~~~~~~

The function has the following parameter:

``address``
   A reference to the data that is to be protected by the monitor. This
   reference must remain valid as long as there are monitoring
   operations being performed.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, the function returns a pointer to the :ref:`PRMonitor`
   associated with the value specified in the ``address`` parameter.
-  If unsuccessful (the monitor cache needs to be expanded and the
   system is out of memory), the function returns ``NULL``.


Description
-----------

:ref:`PR_CEnterMonitor` uses the value specified in the ``address``
parameter to find a monitor in the monitor cache, then enters the lock
associated with the monitor. If no match is found, an available monitor
is associated with the address and the monitor's entry count is
incremented (so it has a value of one). If a match is found, then either
the calling thread is already in the monitor (and this is a reentrant
call) or another thread is holding the monitor's mutex. In the former
case, the entry count is simply incremented and the function returns. In
the latter case, the calling thread is likely to find the monitor locked
by another thread and waits for that thread to exit before continuing.

.. note::

   :ref:`PR_CEnterMonitor` and :ref:`PR_CExitMonitor` must be
   paired--that is, there must be an exit for every entry--or the object
   will never become available for any other thread.
