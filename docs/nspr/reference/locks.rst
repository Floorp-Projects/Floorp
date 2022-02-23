This chapter describes the NSPR API for creation and manipulation of a
mutex of type :ref:`PRLock`.

-  `Lock Type <#Lock_Type>`__
-  `Lock Functions <#Lock_Functions>`__

In NSPR, a mutex of type :ref:`PRLock` controls locking, and associated
condition variables communicate changes in state among threads. When a
programmer associates a mutex with an arbitrary collection of data, the
mutex provides a protective monitor around the data.

In general, a monitor is a conceptual entity composed of a mutex, one or
more condition variables, and the monitored data. Monitors in this
generic sense should not be confused with monitors used in Java
programming. In addition to :ref:`PRLock`, NSPR provides another mutex
type, :ref:`PRMonitor`, which is reentrant and can have only one associated
condition variable. :ref:`PRMonitor` is intended for use with Java and
reflects the Java approach to thread synchronization.

For an introduction to NSPR thread synchronization, including locks and
condition variables, see `Introduction to
NSPR <Introduction_to_NSPR>`__.

For reference information on NSPR condition variables, see `Condition
Variables <Condition_Variables>`__.

.. _Lock_Type:

Lock Type
---------

 - :ref:`PRLock`

.. _Lock_Functions:

Lock Functions
--------------

 - :ref:`PR_NewLock` creates a new lock object.
 - :ref:`PR_DestroyLock` destroys a specified lock object.
 - :ref:`PR_Lock` locks a specified lock object.
 - :ref:`PR_Unlock` unlocks a specified lock object.
