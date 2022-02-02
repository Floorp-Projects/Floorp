This chapter describes the functions you use when you work with cached
monitors. Unlike a plain monitor, a cached monitor is associated with
the address of a protected object, and the association is maintained
only while the protection is needed. This arrangement allows a cached
monitor to be associated with another object without preallocating a
monitor for all objects. A hash table is used to quickly map addresses
to their respective monitors. The system automatically enlarges the hash
table as needed.

Important
---------

Cached monitors are slower to use than their uncached counterparts.

See `Monitors <Monitors>`__ for information about uncached monitors.

.. _Cached_Monitors_Functions:

Cached Monitors Functions
-------------------------

Cached monitors allow the client to associate monitoring protection and
state change synchronization in a lazy fashion. The monitoring
capability is associated with the protected object only during the time
it is required, allowing the monitor object to be reused. This
additional flexibility comes at the cost of a small loss in performance.

 - :ref:`PR_CEnterMonitor` enters the lock associated with a cached
   monitor.
 - :ref:`PR_CExitMonitor` decrements the entry count associated with a
   cached monitor.
 - :ref:`PR_CWait` waits for a notification that a monitor's state has
   changed.
 - :ref:`PR_CNotify` notifies a thread waiting for a change in the state of
   monitored data.
 - :ref:`PR_CNotifyAll` notifies all the threads waiting for a change in
   the state of monitored data.
