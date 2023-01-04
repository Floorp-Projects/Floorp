PR_NotifyCondVar
================

Notifies a condition variable of a change in its associated monitored
data.


Syntax
------

.. code::

   #include <prcvar.h>

   PRStatus PR_NotifyCondVar(PRCondVar *cvar);


Parameter
~~~~~~~~~

:ref:`PR_NotifyCondVar` has one parameter:

``cvar``
   The condition variable to notify.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful (for example, if the caller has not locked the lock
   associated with the condition variable), ``PR_FAILURE``.


Description
-----------

The calling thread must hold the lock that protects the condition, as
well as the invariants that are tightly bound to the condition.

Notification of a condition variable signals a change of state in some
monitored data. When the notification occurs, the runtime promotes a
thread that is waiting on the condition variable to a ready state. If
more than one thread is waiting, the selection of which thread gets
promoted cannot be predicted. This implies that all threads waiting on a
single condition variable must have the same semantics. If no thread is
waiting on the condition variable, the notify operation is a no-op.
