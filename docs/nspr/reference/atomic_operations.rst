This chapter describes the global functions you use to perform atomic
operations. The functions define a portable API that may be reliably
used in any environment. Since not all operating environments provide
access to such functions, their performance may vary considerably.

.. _Atomic_Operations_Functions:

Atomic Operations Functions
---------------------------

The API defined for the atomic functions is consistent across all
supported platforms. However, the implementation may vary greatly, and
hence the performance. On systems that do not provide direct access to
atomic operators, NSPR emulates the capabilities by using its own
locking mechanisms. For such systems, NSPR performs atomic operations
just as efficiently as the client could. Therefore, to preserve
portability, it is recommended that clients use the NSPR API for atomic
operations.

These functions operate on 32-bit integers:

-  :ref:`PR_AtomicIncrement`
-  :ref:`PR_AtomicDecrement`
-  :ref:`PR_AtomicSet`
-  :ref:`PR_AtomicAdd`

These functions implement a simple stack data structure:

-  :ref:`PR_CreateStack`
-  :ref:`PR_StackPush`
-  :ref:`PR_StackPop`
-  :ref:`PR_DestroyStack`
