PR_Notify
=========

Notifies a monitor that a change in state of the monitored data has
occurred.


Syntax
------

.. code::

   #include <prmon.h>

   PRStatus PR_Notify(PRMonitor *mon);


Parameters
~~~~~~~~~~

The function has the following parameter:

``mon``
   A reference to an existing structure of type :ref:`PRMonitor`. The
   monitor object referenced must be one for which the calling thread
   currently holds the lock.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``.


Description
-----------

Notification of a monitor signals the change of state of some monitored
data. The changing of that data and the notification must all be
performed while in the monitor. When the notification occurs, the
runtime promotes a thread that is waiting on the monitor to a ready
state. If more than one thread is waiting, the selection of which thread
gets promoted cannot be determined in advance. This implies that all
threads waiting on a single monitor must have the same semantics. If no
thread is waiting on the monitor, the notify operation is a no-op.
