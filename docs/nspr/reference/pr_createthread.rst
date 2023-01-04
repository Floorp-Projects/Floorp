PR_CreateThread
===============

Creates a new thread.


Syntax
------

.. code::

   #include <prthread.h>

   PRThread* PR_CreateThread(
      PRThreadType type,
      void (*start)(void *arg),
      void *arg,
      PRThreadPriority priority,
      PRThreadScope scope,
      PRThreadState state,
      PRUint32 stackSize);


Parameters
~~~~~~~~~~

:ref:`PR_CreateThread` has the following parameters:

``type``
   Specifies that the thread is either a user thread
   (``PR_USER_THREAD``) or a system thread (``PR_SYSTEM_THREAD``).
``start``
   A pointer to the thread's root function, which is called as the root
   of the new thread. Returning from this function is the only way to
   terminate a thread.
``arg``
   A pointer to the root function's only parameter. NSPR does not assess
   the type or the validity of the value passed in this parameter.
``priority``
   The initial priority of the newly created thread.
``scope``
   Specifies your preference for making the thread local
   (``PR_LOCAL_THREAD``), global (``PR_GLOBAL_THREAD``) or global bound
   (``PR_GLOBAL_BOUND_THREAD``). However, NSPR may override this
   preference if necessary.
``state``
   Specifies whether the thread is joinable (``PR_JOINABLE_THREAD``) or
   unjoinable (``PR_UNJOINABLE_THREAD``).
``stackSize``
   Specifies your preference for the size of the stack, in bytes,
   associated with the newly created thread. If you pass zero in this
   parameter, :ref:`PR_CreateThread` chooses the most favorable
   machine-specific stack size.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, a pointer to the new thread. This pointer remains
   valid until the thread returns from its root function.
-  If unsuccessful, (for example, if system resources are unavailable),
   ``NULL``.


Description
-----------

If you want the thread to start up waiting for the creator to do
something, enter a lock before creating the thread and then have the
thread's root function enter and exit the same lock. When you are ready
for the thread to run, exit the lock. For more information on locks and
thread synchronization, see `Introduction to
NSPR <Introduction_to_NSPR>`__.

If you want to detect the completion of the created thread, make it
joinable. You can then use :ref:`PR_JoinThread` to synchronize the
termination of another thread.
