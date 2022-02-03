NSPR provides an execution environment that promotes the use of
lightweight threads. Each thread is an execution entity that is
scheduled independently from other threads in the same process. This
chapter describes the basic NSPR threading API.

-  `Threading Types and Constants <#Threading_Types_and_Constants>`__
-  `Threading Functions <#Threading_Functions>`__

A thread has a limited number of resources that it truly owns. These
resources include a stack and the CPU registers (including PC). To an
NSPR client, a thread is represented by a pointer to an opaque structure
of type :ref:`PRThread`. A thread is created by an explicit client request
and remains a valid, independent execution entity until it returns from
its root function or the process abnormally terminates. Threads are
critical resources and therefore require some management. To synchronize
the termination of a thread, you can **join** it with another thread
(see :ref:`PR_JoinThread`). Joining a thread provides definitive proof that
the target thread has terminated and has finished with both the
resources to which the thread has access and the resources of the thread
itself.

For an overview of the NSPR threading model and sample code that
illustrates its use, see `Introduction to
NSPR <Introduction_to_NSPR>`__.

For API reference information related to thread synchronization, see
`Locks <Locks>`__ and `Condition Variables <Condition_Variables>`__.

.. _Threading_Types_and_Constants:

Threading Types and Constants
-----------------------------

 - :ref:`PRThread`
 - :ref:`PRThreadType`
 - :ref:`PRThreadScope`
 - :ref:`PRThreadState`
 - :ref:`PRThreadPriority`
 - :ref:`PRThreadPrivateDTOR`

.. _Threading_Functions:

Threading Functions
-------------------

Most of the functions described here accept a pointer to the thread as
an argument. NSPR does not check for the validity of the thread. It is
the caller's responsibility to ensure that the thread is valid. The
effects of these functions on invalid threads are undefined.

-  `Creating, Joining, and Identifying
   Threads <#Creating,_Joining,_and_Identifying_Threads>`__
-  `Controlling Thread Priorities <#Controlling_Thread_Priorities>`__
-  `Interrupting and Yielding <#Interrupting_and_Yielding>`__
-  `Setting Global Thread
   Concurrency <#Setting_Global_Thread_Concurrency>`__
-  `Getting a Thread's Scope <#Getting_a_Thread's_Scope>`__

.. _Creating.2C_Joining.2C_and_Identifying_Threads:

Creating, Joining, and Identifying Threads
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_CreateThread` creates a new thread.
 - :ref:`PR_JoinThread` blocks the calling thread until a specified thread
   terminates.
 - :ref:`PR_GetCurrentThread` returns the current thread object for the
   currently running code.
 - :ref:`PR_AttachThread`` associates a :ref:`PRThread` object with an existing
   native thread.
 - :ref:`PR_DetachThread`` disassociates a :ref:`PRThread` object from a native
   thread.

.. _Controlling_Thread_Priorities:

Controlling Thread Priorities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For an overview of the way NSPR controls thread priorities, see `Setting
Thread Priorities <Introduction_to_NSPR#Setting_Thread_Priorities.>`__.

You set a thread's NSPR priority when you create it with
:ref:`PR_CreateThread`. After a thread has been created, you can get and
set its priority with these functions:

 - :ref:`PR_GetThreadPriority`
 - :ref:`PR_SetThreadPriority`

.. _Controlling_Per-Thread_Private_Data:

Controlling Per-Thread Private Data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can use these functions to associate private data with each of the
threads in a process:

 - :ref:`PR_NewThreadPrivateIndex` allocates a unique index. If the call is
   successful, every thread in the same process is capable of
   associating private data with the new index.
 - :ref:`PR_SetThreadPrivate` associates private thread data with an index.
 - :ref:`PR_GetThreadPrivate` retrieves data associated with an index.

.. _Interrupting_and_Yielding:

Interrupting and Yielding
~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_Interrupt` requests an interrupt of another thread. Once the
   target thread has been notified of the request, the request stays
   with the thread until the notification either has been delivered
   exactly once or is cleared.
 - :ref:`PR_ClearInterrupt` clears a previous interrupt request.
 - :ref:`PR_Sleep` causes a thread to yield to other threads for a
   specified number of ticks.

.. _Setting_Global_Thread_Concurrency:

Setting Global Thread Concurrency
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_SetConcurrency` sets the number of global threads used by NSPR
   to create local threads.

.. _Getting_a_Thread.27s_Scope:

Getting a Thread's Scope
~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_GetThreadScope` gets the scoping of the current thread.
