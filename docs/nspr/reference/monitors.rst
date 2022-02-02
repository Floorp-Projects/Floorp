In addition to the mutex type :ref:`PRLock`, NSPR provides a special type,
:ref:`PRMonitor`, for use in Java programming. This chapter describes the
NSPR API for creation and manipulation of a mutex of type :ref:`PRMonitor`.

-  `Monitor Type <#Monitor_Type>`__
-  `Monitor Functions <#Monitor_Functions>`__

With a mutex of type :ref:`PRLock`, a single thread may enter the monitor
only once before it exits, and the mutex can have multiple associated
condition variables.

With a mutex of type :ref:`PRMonitor`, a single thread may re-enter a
monitor as many times as it sees fit. The first time the thread enters a
monitor, it acquires the monitor's lock and the thread's entry count is
incremented to 1. Each subsequent time the thread successfully enters
the same monitor, the thread's entry count is incremented again, and
each time the thread exits the monitor, the thread's entry count is
decremented. When the entry count for a thread reaches zero, the thread
releases the monitor's lock, and other threads that were blocked while
trying to enter the monitor will be rescheduled.

A call to :ref:`PR_Wait` temporarily returns the entry count to zero. When
the calling thread resumes, it has the same entry count it had before
the wait operation.

Unlike a mutex of type :ref:`PRLock`, a mutex of type :ref:`PRMonitor` has a
single, implicitly associated condition variable that may be used to
facilitate synchronization of threads with the change in state of
monitored data.

For an introduction to NSPR thread synchronization, including locks and
condition variables, see `Introduction to
NSPR <Introduction_to_NSPR>`__.

.. _Monitor_Type:

Monitor Type
------------

With the exception of :ref:`PR_NewMonitor`, which creates a new monitor
object, all monitor functions require a pointer to an opaque object of
type :ref:`PRMonitor`.

.. _Monitor_Functions:

Monitor Functions
-----------------

All monitor functions are thread-safe. However, this safety does not
extend to protecting the monitor object from deletion.

 - :ref:`PR_NewMonitor` creates a new monitor.
 - :ref:`PR_DestroyMonitor` destroys a monitor object.
 - :ref:`PR_EnterMonitor` enters the lock associated with a specified
   monitor.
 - :ref:`PR_ExitMonitor` decrements the entry count associated with a
   specified monitor.
 - :ref:`PR_Wait` waits for a notify on a specified monitor's condition
   variable.
 - :ref:`PR_Notify` notifies a thread waiting on a specified monitor's
   condition variable.
 - :ref:`PR_NotifyAll` notifies all threads waiting on a specified
   monitor's condition variable.
