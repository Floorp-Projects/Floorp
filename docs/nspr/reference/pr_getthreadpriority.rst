PR_GetThreadPriority
====================

Returns the priority of a specified thread.


Syntax
------

.. code::

   #include <prthread.h>

   PRThreadPriority PR_GetThreadPriority(PRThread *thread);


Parameter
~~~~~~~~~

:ref:`PR_GetThreadPriority` has the following parameter:

``thread``
   A valid identifier for the thread whose priority you want to know.
