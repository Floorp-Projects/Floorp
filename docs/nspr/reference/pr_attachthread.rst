PR_AttachThread
===============

.. container:: blockIndicator obsolete obsoleteHeader

   | **Obsolete**
   | This feature is obsolete. Although it may still work in some
     browsers, its use is discouraged since it could be removed at any
     time. Try to avoid using it.

Associates a :ref:`PRThread` object with an existing native thread.


Syntax
------

.. code::

   #include <pprthread.h>

   PRThread* PR_AttachThread(
      PRThreadType type,
      PRThreadPriority priority,
      PRThreadStack *stack);


Parameters
~~~~~~~~~~

:ref:`PR_AttachThread` has the following parameters:

``type``
   Specifies that the thread is either a user thread
   (``PR_USER_THREAD``) or a system thread (``PR_SYSTEM_THREAD``).
``priority``
   The priority to assign to the thread being attached.
``stack``
   The stack for the thread being attached.


Returns
~~~~~~~

The function returns one of these values:

-  If successful, a pointer to a :ref:`PRThread` object.
-  If unsuccessful, for example if system resources are not available,
   ``NULL``.


Description
-----------

You use :ref:`PR_AttachThread` when you want to use NSS functions on the
native thread that was not created with NSPR. :ref:`PR_AttachThread`
informs NSPR about the new thread by associating a :ref:`PRThread` object
with the native thread.

The thread object is automatically destroyed when it is no longer
needed.

You don't need to call :ref:`PR_AttachThread` unless you create your own
native thread. :ref:`PR_Init` calls :ref:`PR_AttachThread` automatically for
the primordial thread.

.. note::

   As of NSPR release v3.0, :ref:`PR_AttachThread` and
   :ref:`PR_DetachThread` are obsolete. A native thread not created by NSPR
   is automatically attached the first time it calls an NSPR function,
   and automatically detached when it exits.

In NSPR release 19980529B and earlier, it is necessary for a native
thread not created by NSPR to call :ref:`PR_AttachThread` before it calls
any NSPR functions, and call :ref:`PR_DetachThread` when it is done calling
NSPR functions.
