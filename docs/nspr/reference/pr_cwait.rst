PR_CWait
========

Wait for a notification that a monitor's state has changed.


Syntax
------

.. code::

   #include <prcmon.h>

   PRStatus PR_CWait(
      void *address,
      PRIntervalTime timeout);


Parameters
~~~~~~~~~~

The function has the following parameters:

``address``
   The address of the protected object--the same address previously
   passed to :ref:`PR_CEnterMonitor`.
``timeout``
   The amount of time (in :ref:`PRIntervalTime` units) that the thread is
   willing to wait for an explicit notification before being
   rescheduled. If you specify ``PR_INTERVAL_NO_TIMEOUT``, the function
   returns if and only if the object is notified.


Returns
~~~~~~~

The function returns one of the following values:

 - :ref:`PR_SUCCESS` indicates either that the monitored object has been
   notified or that the interval specified in the timeout parameter has
   been exceeded.
 - :ref:`PR_FAILURE` indicates either that the monitor could not be located
   in the cache or that the monitor was located and the calling thread
   was not the thread that held the monitor's mutex.


Description
-----------

Using the value specified in the ``address`` parameter to find a monitor
in the monitor cache, :ref:`PR_CWait` waits for a notification that the
monitor's state has changed. While the thread is waiting, it exits the
monitor (just as if it had called :ref:`PR_CExitMonitor` as many times as
it had called :ref:`PR_CEnterMonitor`). When the wait has finished, the
thread regains control of the monitor's lock with the same entry count
as before the wait began.

The thread waiting on the monitor resumes execution when the monitor is
notified (assuming the thread is the next in line to receive the notify)
or when the interval specified in the ``timeout`` parameter has been
exceeded. When the thread resumes execution, it is the caller's
responsibility to test the state of the monitored data to determine the
appropriate action.
