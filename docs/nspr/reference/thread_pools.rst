This chapter describes the NSPR API Thread Pools.

.. note::

   **Note:** This API is a preliminary version in NSPR 4.0 and is
   subject to change.

Thread pools create and manage threads to provide support for scheduling
work (jobs) onto one or more threads. NSPR's thread pool is modeled on
the thread pools described by David R. Butenhof in\ *Programming with
POSIX Threads* (Addison-Wesley, 1997).

-  `Thread Pool Types <#Thread_Pool_Types>`__
-  `Thread Pool Functions <#Thread_Pool_Functions>`__

.. _Thread_Pool_Types:

Thread Pool Types
-----------------

 - :ref:`PRJobIoDesc`
 - :ref:`PRJobFn`
 - :ref:`PRThreadPool`
 - :ref:`PRJob`

.. _Thread_Pool_Functions:

Thread Pool Functions
---------------------

 - :ref:`PR_CreateThreadPool`
 - :ref:`PR_QueueJob`
 - :ref:`PR_QueueJob_Read`
 - :ref:`PR_QueueJob_Write`
 - :ref:`PR_QueueJob_Accept`
 - :ref:`PR_QueueJob_Connect`
 - :ref:`PR_QueueJob_Timer`
 - :ref:`PR_CancelJob`
 - :ref:`PR_JoinJob`
 - :ref:`PR_ShutdownThreadPool`
 - :ref:`PR_JoinThreadPool`
