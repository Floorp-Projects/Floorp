PR_JoinThread
=============

Blocks the calling thread until a specified thread terminates.


Syntax
------

.. code::

   #include <prthread.h>

   PRStatus PR_JoinThread(PRThread *thread);


Parameter
~~~~~~~~~

:ref:`PR_JoinThread` has the following parameter:

``thread``
   A valid identifier for the thread that is to be joined.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``
-  If unsuccessful--for example, if no joinable thread can be found that
   corresponds to the specified target thread, or if the target thread
   is unjoinable--``PR_FAILURE``.


Description
-----------

:ref:`PR_JoinThread` is used to synchronize the termination of a thread.
The function is synchronous in that it blocks the calling thread until
the target thread is in a joinable state. :ref:`PR_JoinThread` returns to
the caller only after the target thread returns from its root function.

:ref:`PR_JoinThread` must not be called until after :ref:`PR_CreateThread` has
returned.  If :ref:`PR_JoinThread` is not called on the same thread as
:ref:`PR_CreateThread`, then it is the caller's responsibility to ensure
that :ref:`PR_CreateThread` has completed.

Several threads cannot wait for the same thread to complete. One of the
calling threads operates successfully, and the others terminate with the
error ``PR_FAILURE``.

The calling thread is not blocked if the target thread has already
terminated.

:ref:`PR_JoinThread` is interruptible.
