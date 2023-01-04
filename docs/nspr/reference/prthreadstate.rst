PRThreadState
=============

A thread's thread state is either joinable or unjoinable.


Syntax
------

.. code::

   #include <prthread.h>

   typedef enum PRThreadState {
      PR_JOINABLE_THREAD,
      PR_UNJOINABLE_THREAD
   } PRThreadState;


Enumerators
~~~~~~~~~~~

``PR_UNJOINABLE_THREAD``
   Thread termination happens implicitly when the thread returns from
   the root function. The time of release of the resources assigned to
   the thread cannot be determined in advance. Threads created with a
   ``PR_UNJOINABLE_THREAD`` state cannot be used as arguments to
   :ref:`PR_JoinThread`.
``PR_JOINABLE_THREAD``
   Joinable thread references remain valid after they have returned from
   their root function until :ref:`PR_JoinThread` is called. This approach
   facilitates management of the process' critical resources.


Description
-----------

A thread is a critical resource and must be managed.

The lifetime of a thread extends from the time it is created to the time
it returns from its root function. What happens when it returns from its
root function depends on the thread state passed to :ref:`PR_CreateThread`
when the thread was created.

If a thread is created as a joinable thread, it continues to exist after
returning from its root function until another thread joins it. The join
process permits strict synchronization of thread termination and
therefore promotes effective resource management.

If a thread is created as an unjoinable (also called detached) thread,
it terminates and cleans up after itself after returning from its root
function. This results in some ambiguity after the thread's root
function has returned and before the thread has finished terminating
itself.
