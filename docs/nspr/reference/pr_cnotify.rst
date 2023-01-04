PR_CNotify
==========

Notify a thread waiting on a change in the state of monitored data.


Syntax
------

.. code::

   #include <prcmon.h>

   PRStatus PR_CNotify(void *address);


Parameter
~~~~~~~~~

The function has the following parameter:

``address``
   The address of the monitored object. The calling thread must be in
   the monitor defined by the value of the address.


Returns
~~~~~~~

 - :ref:`PR_SUCCESS` indicates that the calling thread is the holder of the
   mutex for the monitor referred to by the address parameter.
 - :ref:`PR_FAILURE` indicates that the monitor has not been entered by the
   calling thread.


Description
-----------

Using the value specified in the ``address`` parameter to find a monitor
in the monitor cache, :ref:`PR_CNotify` notifies single a thread waiting
for the monitor's state to change. If a thread is waiting on the monitor
(having called :ref:`PR_CWait`), then that thread is made ready. As soon as
the thread is scheduled, it attempts to reenter the monitor.
