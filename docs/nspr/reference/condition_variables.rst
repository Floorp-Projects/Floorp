This chapter describes the API for creating and destroying condition
variables, notifying condition variables of changes in monitored data,
and making a thread wait on such notification.

-  `Condition Variable Type <#Condition_Variable_Type>`__
-  `Condition Variable Functions <#Condition_Variable_Functions>`__

Conditions are closely associated with a single monitor, which typically
consists of a mutex, one or more condition variables, and the monitored
data. The association between a condition and a monitor is established
when a condition variable is created, and the association persists for
its life. In addition, a static association exists between the condition
and some data within the monitor. This data is what will be manipulated
by the program under the protection of the monitor.

A call to :ref:`PR_WaitCondVar` causes a thread to block until a specified
condition variable receives notification of a change of state in its
associated monitored data. Other threads may notify the condition
variable when changes occur.

For an introduction to NSPR thread synchronization, including locks and
condition variables, see `Introduction to
NSPR <Introduction_to_NSPR>`__.

For reference information on NSPR locks, see
`Locks <NSPR_API_Reference/Locks>`__.

NSPR provides a special type, :ref:`PRMonitor`, for use with Java. Unlike a
mutex of type :ref:`PRLock`, which can have multiple associated condition
variables of type :ref:`PRCondVar`, a mutex of type :ref:`PRMonitor` has a
single, implicitly associated condition variable. For information about
:ref:`PRMonitor`, see `Monitors <Monitors>`__.

.. _Condition_Variable_Type:

Condition Variable Type
-----------------------

 - :ref:`PRCondVar`

.. _Condition_Variable_Functions:

Condition Variable Functions
----------------------------

 - :ref:`PR_NewCondVar`
 - :ref:`PR_DestroyCondVar`
 - :ref:`PR_WaitCondVar`
 - :ref:`PR_NotifyCondVar`
 - :ref:`PR_NotifyAllCondVar`
