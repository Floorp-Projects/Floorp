PR_GetThreadPrivate
===================

Recovers the per-thread private data for the current thread.


Syntax
------

.. code::

   #include <prthread.h>

   void* PR_GetThreadPrivate(PRUintn index);


Parameter
~~~~~~~~~

:ref:`PR_GetThreadPrivate` has the following parameters:

``index``
   The index into the per-thread private data table.


Returns
~~~~~~~

``NULL`` if the data has not been set.


Description
-----------

:ref:`PR_GetThreadPrivate` may be called at any time during a thread's
execution. A thread can get access only to its own per-thread private
data. Do not delete the object that the private data refers to without
first clearing the thread's value.
