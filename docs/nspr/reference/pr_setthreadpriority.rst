PR_SetThreadPriority
====================

Sets the priority of a specified thread.


Syntax
------

.. code::

   #include <prthread.h>

   void PR_SetThreadPriority(
      PRThread *thread,
      PRThreadPriority priority);


Parameters
~~~~~~~~~~

:ref:`PR_SetThreadPriority` has the following parameters:

``thread``
   A valid identifier for the thread whose priority you want to set.
``priority``
   The priority you want to set.


Description
-----------

Modifying the priority of a thread other than the calling thread is
risky. It is difficult to ensure that the state of the target thread
permits a priority adjustment without ill effects. It is preferable for
a thread to specify itself in the thread parameter when it calls
:ref:`PR_SetThreadPriority`.
