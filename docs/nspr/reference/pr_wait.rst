PR_Wait
=======

Waits for an application-defined state of the monitored data to exist.


Syntax
------

.. code::

   #include <prmon.h>

   PRStatus PR_Wait(
     PRMonitor *mon,
     PRIntervalTime ticks);


Parameters
~~~~~~~~~~

The function has the following parameter:

``mon``
   A reference to an existing structure of type :ref:`PRMonitor`. The
   monitor object referenced must be one for which the calling thread
   currently holds the lock.
``ticks``
   The amount of time (in :ref:`PRIntervalTime` units) that the thread is
   willing to wait for an explicit notification before being
   rescheduled.


Returns
~~~~~~~

The function returns one of the following values:

 - :ref:`PR_SUCCESS`` means the thread is being resumed from the ``PR_Wait`
   call either because it was explicitly notified or because the time
   specified by the parameter ``ticks`` has expired.
 - :ref:`PR_FAILURE` means ``PR_Wait`` encountered a system error (such as
   an invalid monitor reference) or the thread was interrupted by
   another thread.


Description
-----------

A call to :ref:`PR_Wait` causes the thread to release the monitor's lock,
just as if it had called :ref:`PR_ExitMonitor` as many times as it had
called :ref:`PR_EnterMonitor`. This has the effect of making the monitor
available to other threads. When the wait is over, the thread regains
control of the monitor's lock with the same entry count it had before
the wait began.

A thread waiting on the monitor resumes when the monitor is notified or
when the timeout specified by the ``ticks`` parameter elapses. The
resumption from the wait is merely a hint that a change of state has
occurred. It is the responsibility of the programmer to evaluate the
data and act accordingly. This is usually done by evaluating a Boolean
expression involving the monitored data. While the Boolean expression is
false, the thread should wait. The thread should act on the data only
when the expression is true. The boolean expression must be evaluated
while in the monitor and within a loop.

In pseudo-code, the sequence is as follows:

| ``PR_EnterMonitor(&ml);``
| ``while (!expression) wait;``
| ``... act on the state change ...``
| ``PR_ExitMonitor(&ml);``

A thread can be resumed from a wait for a variety of reasons. The most
obvious is that it was notified by another thread. If the value of
timeout is not ``PR_INTERVAL_NO_TIMEOUT``, :ref:`PR_Wait` resumes execution
after the specified interval has expired. If a timeout value is used,
the Boolean expression must include elapsed time as part of the
monitored data.

Resuming from the wait is merely an opportunity to evaluate the
expression, not an assertion that the expression is true.
