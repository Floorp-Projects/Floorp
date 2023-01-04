PR_CExitMonitor
===============

Decrement the entry count associated with a cached monitor.


Syntax
------

.. code::

   #include <prcmon.h>

   PRStatus PR_CExitMonitor(void *address);


Parameters
~~~~~~~~~~

The function has the following parameters:

``address``
   The address of the protected object--the same address previously
   passed to :ref:`PR_CEnterMonitor`.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. This may indicate that the address
   parameter is invalid or that the calling thread is not in the
   monitor.


Description
-----------

Using the value specified in the address parameter to find a monitor in
the monitor cache, :ref:`PR_CExitMonitor` decrements the entry count
associated with the monitor. If the decremented entry count is zero, the
monitor is exited.
