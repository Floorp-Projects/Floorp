PR_WaitCondVar
==============

Waits on a condition.


Syntax
------

.. code::

   #include <prcvar.h>

   PRStatus PR_WaitCondVar(
     PRCondVar *cvar,
     PRIntervalTime timeout);


Parameters
~~~~~~~~~~

:ref:`PR_WaitCondVar` has the following parameters:

``cvar``
   The condition variable on which to wait.
``timeout``
   The value ``PR_INTERVAL_NO_TIMEOUT`` requires that a condition be
   notified (or the thread interrupted) before it will resume from the
   wait. The value ``PR_INTERVAL_NO_WAIT`` causes the thread to release
   the lock, possibly causing a rescheduling within the runtime, then
   immediately attempt to reacquire the lock and resume.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful (for example, if the caller has not locked the lock
   associated with the condition variable or the thread was interrupted
   with :ref:`PR_Interrupt`), ``PR_FAILURE``. The details can be determined
   with :ref:`PR_GetError`.


Description
-----------

Before the call to :ref:`PR_WaitCondVar`, the lock associated with the
condition variable must be held by the calling thread. After a call to
:ref:`PR_WaitCondVar`, the lock is released and the thread is blocked in a
"waiting on condition" state until another thread notifies the condition
or a caller-specified amount of time expires.

When the condition variable is notified, a thread waiting on that
condition moves from the "waiting on condition" state to the "ready"
state. When scheduled, the thread attempts to reacquire the lock that it
held when :ref:`PR_WaitCondVar` was called.

Any value other than ``PR_INTERVAL_NO_TIMEOUT`` or
``PR_INTERVAL_NO_WAIT`` for the timeout parameter will cause the thread
to be rescheduled due to either explicit notification or the expiration
of the specified interval. The latter must be determined by treating
time as one part of the monitored data being protected by the lock and
tested explicitly for an expired interval. To detect the expiration of
the specified interval, call :ref:`PR_IntervalNow` before and after the
call to :ref:`PR_WaitCondVar` and compare the elapsed time with the
specified interval.
