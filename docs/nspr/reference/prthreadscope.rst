PRThreadScope
=============

The scope of an NSPR thread, specified as a parameter to
:ref:`PR_CreateThread` or returned by :ref:`PR_GetThreadScope`.


Syntax
------

.. code::

   #include <prthread.h>

   typedef enum PRThreadScope {
      PR_LOCAL_THREAD,
      PR_GLOBAL_THREAD
      PR_GLOBAL_BOUND_THREAD
   } PRThreadScope;


Enumerators
~~~~~~~~~~~

``PR_LOCAL_THREAD``
   A local thread, scheduled locally by NSPR within the process.
``PR_GLOBAL_THREAD``
   A global thread, scheduled by the host OS.
``PR_GLOBAL_BOUND_THREAD``
   A global bound (kernel) thread, scheduled by the host OS


Description
-----------

An enumerator of type :ref:`PRThreadScope` specifies how a thread is
scheduled: either locally by NSPR within the process (a local thread) or
globally by the host (a global thread).

Global threads are scheduled by the host OS and compete with all other
threads on the host OS for resources. They are subject to fairly
sophisticated scheduling techniques.

Local threads are scheduled by NSPR within the process. The process is
assumed to be globally scheduled, but NSPR can manipulate local threads
without system intervention. In most cases, this leads to a significant
performance benefit.

However, on systems that require NSPR to make a distinction between
global and local threads, global threads are invariably required to do
any form of I/O. If a thread is likely to do a lot of I/O, making it a
global thread early is probably warranted.

On systems that don't make a distinction between local and global
threads, NSPR silently ignores the scheduling request. To find the scope
of the thread, call :ref:`PR_GetThreadScope`.
