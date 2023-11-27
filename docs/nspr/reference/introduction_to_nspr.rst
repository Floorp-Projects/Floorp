Introduction to NSPR
====================

The Netscape Portable Runtime (NSPR) API allows compliant applications
to use system facilities such as threads, thread synchronization, I/O,
interval timing, atomic operations, and several other low-level services
in a platform-independent manner. This chapter introduces key NSPR
programming concepts and illustrates them with sample code.

NSPR does not provide a platform for porting existing code. It must be
used from the beginning of a software project.

.. _NSPR_Naming_Conventions:

NSPR Naming Conventions
-----------------------

Naming of NSPR types, functions, and macros follows the following
conventions:

-  Types exported by NSPR begin with ``PR`` and are followed by
   intercap-style declarations, like this: :ref:`PRInt`, :ref:`PRFileDesc`
-  Function definitions begin with ``PR_`` and are followed by
   intercap-style declarations, like this: :ref:`PR_Read``,
   :ref:`PR_JoinThread``
-  Preprocessor macros begin with the letters ``PR`` and are followed by
   all uppercase characters separated with the underscore character
   (``_``), like this: :ref:`PR_BYTES_PER_SHORT`, :ref:`PR_EXTERN`

.. _NSPR_Threads:

NSPR Threads
------------

NSPR provides an execution environment that promotes the use of
lightweight threads. Each thread is an execution entity that is
scheduled independently from other threads in the same process. A thread
has a limited number of resources that it truly owns. These resources
include the thread stack and the CPU register set (including PC).

To an NSPR client, a thread is represented by a pointer to an opaque
structure of type :ref:`PRThread``. A thread is created by an explicit
client request and remains a valid, independent execution entity until
it returns from its root function or the process abnormally terminates.
(:ref:`PRThread` and functions for creating and manipulating threads are
described in detail in `Threads <Threads>`__.)

NSPR threads are lightweight in the sense that they are cheaper than
full-blown processes, but they are not free. They achieve the cost
reduction by relying on their containing process to manage most of the
resources that they access. This, and the fact that threads share an
address space with other threads in the same process, makes it important
to remember that *threads are not processes* .

NSPR threads are scheduled in two separate domains:

-  **Local threads** are scheduled within a process only and are handled
   entirely by NSPR, either by completely emulating threads on each host
   operating system (OS) that doesn't support threads, or by using the
   threading facilities of each host OS that does support threads to
   emulate a relatively large number of local threads by using a
   relatively small number of native threads.

-  **Global threads** are scheduled by the host OS--not by NSPR--either
   within a process or across processes on the entire host. Global
   threads correspond to native threads on the host OS.

NSPR threads can also be either user threads or system threads. NSPR
provides a function, :ref:`PR_Cleanup`, that synchronizes process
termination. :ref:`PR_Cleanup` waits for the last user thread to exit
before returning, whereas it ignores system threads when determining
when a process should exit. This arrangement implies that a system
thread should not have volatile data that needs to be safely stored
away.

Priorities for NSPR threads are based loosely on hints provided by the
client and sometimes constrained by the underlying operating system.
Therefore, priorities are not rigidly defined. For more information, see
`Thread Scheduling <#Thread_Scheduling>`__.

In general, it's preferable to create local user threads with normal
priority and let NSPR take care of the details as appropriate for each
host OS. It's usually not necessary to create a global thread explicitly
unless you are planning to port your code only to platforms that provide
threading services with which you are familiar or unless the thread will
be executing code that might directly call blocking OS functions.

Threads can also have "per-thread-data" attached to them. Each thread
has a built-in per-thread error number and error string that are updated
when NSPR operations fail. It's also possible for NSPR clients to define
their own per-thread-data. For details, see `Controlling Per-Thread
Private Data <Threads#Controlling_Per-Thread_Private_Data>`__.

.. _Thread_Scheduling:

Thread Scheduling
~~~~~~~~~~~~~~~~~

NSPR threads are scheduled by priority and can be preempted or
interrupted. The sections that follow briefly introduce the NSPR
approach to these three aspects of thread scheduling.

-  `Setting Thread Priorities <#Setting_Thread_Priorities>`__
-  `Preempting Threads <#Preempting_Threads>`__
-  `Interrupting Threads <#Interrupting_Threads>`__

For reference information on the NSPR API used for thread scheduling,
see `Threads <Threads>`__.

.. _Setting_Thread_Priorities:

Setting Thread Priorities
^^^^^^^^^^^^^^^^^^^^^^^^^

The host operating systems supported by NSPR differ widely in the
mechanisms they use to support thread priorities. In general, an NSPR
thread of higher priority has a statistically better chance of running
relative to threads of lower priority. However, because of the multiple
strategies to provide execution vehicles for threads on various host
platforms, priorities are not a clearly defined abstraction in NSPR. At
best they are intended to specify a preference with respect to the
amount of CPU time that a higher-priority thread might expect relative
to a lower-priority thread. This preference is still subject to resource
availability, and must not be used in place of proper synchronization.
For more information on thread synchronization, see `NSPR Thread
Synchronization <#NSPR_Thread_Synchronization>`__.

The issue is further muddied by inconsistent offerings from OS vendors
regarding the priority of their kernel-supported threads. NSPR assumes
that the priorities of global threads are not manageable, but that the
host OS will perform some sort of fair scheduling. It's usually
preferable to create local user threads with normal priority and let
NSPR and the host take care of the details.

In some NSPR configurations, there may be an arbitrary (and perhaps
large) number of local threads being supported by a more limited number
of **virtual processors** (an internal application of global threads).
In such situations, each virtual processor will have some number of
local threads associated with it, though exactly which local threads and
how many may vary over time. NSPR guarantees that for each virtual
processor the highest-priority, schedulable local thread is the one
executing. This thread implementation strategy is referred to as the **M
x N model.**

.. _Preempting_Threads:

Preempting Threads
^^^^^^^^^^^^^^^^^^

Preemption is the act of taking control away from a ready thread at an
arbitrary point and giving control to another appropriate thread. It
might be viewed as taking the executing thread and adding it to the end
of the ready queue for its appropriate priority, then simply running the
scheduling algorithm to find the most appropriate thread. The chosen
thread may be of higher priority, of the same priority, or even the same
thread. It will not be a thread of lower priority.

Some operating systems cannot be made preemptible (for example, Mac OS
and Win 16). This puts them at some risk in supporting arbitrary code,
even if the code is interpreted (Java). Other systems are not
thread-aware, and their runtime libraries not thread-safe (most versions
of Unix). These systems can support local level thread abstractions that
can be made preemptible, but run the risk of library corruption
(``libc``). Still other operating systems have a native notion of
threads, and their libraries are thread-aware and support locking.
However, if local threads are also present, and they are preemptible,
they are subject to deadlock. At this time, the only safe solutions are
to turn off preemption (a runtime decision) or to preempt global threads
only.

.. _Interrupting_Threads:

Interrupting Threads
^^^^^^^^^^^^^^^^^^^^

NSPR threads are interruptible, with some constraints and
inconsistencies.

To interrupt a thread, the caller of :ref:`PR_Interrupt` must have the NSPR
reference to the target thread (:ref:`PRThread`). When the target is
interrupted, it is rescheduled from the point at which it was blocked,
with a status error indicating that it was interrupted. NSPR recognizes
only two areas where a thread may be interrupted: waiting on a condition
variable and waiting on I/O. In the latter case, interruption does
cancel the I/O operation. In neither case does being interrupted imply
the demise of the thread.

.. _NSPR_Thread_Synchronization:

NSPR Thread Synchronization
---------------------------

Thread synchronization has two aspects: locking and notification.
Locking prevents access to some resource, such as a piece of shared
data: that is, it enforces mutual exclusion. Notification involves
passing synchronization information among cooperating threads.

In NSPR, a **mutual exclusion lock** (or **mutex**) of type :ref:`PRLock`
controls locking, and associated **condition variables** of type
:ref:`PRCondVar` communicate changes in state among threads. When a
programmer associates a mutex with an arbitrary collection of data, the
mutex provides a protective **monitor** around the data.

.. _Locks_and_Monitors:

Locks and Monitors
~~~~~~~~~~~~~~~~~~

In general, a monitor is a conceptual entity composed of a mutex, one or
more condition variables, and the monitored data. Monitors in this
generic sense should not be confused with the monitor type used in Java
programming. In addition to :ref:`PRLock`, NSPR provides another mutex
type, :ref:`PRMonitor`, which is reentrant and can have only one associated
condition variable. :ref:`PRMonitor` is intended for use with Java and
reflects the Java approach to thread synchronization.

To access the data in the monitor, the thread performing the access must
hold the mutex, also described as being "in the monitor." Mutual
exclusion guarantees that only one thread can be in the monitor at a
time and that no thread may observe or modify the monitored data without
being in the monitor.

Monitoring is about protecting data, not code. A **monitored invariant**
is a Boolean expression over the monitored data. The expression may be
false only when a thread is in the monitor (holding the monitor's
mutex). This requirement implies that when a thread first enters the
monitor, an evaluation of the invariant expression must yield a
``true``. The thread must also reinstate the monitored invariant before
exiting the monitor. Therefore, evaluation of the expression must also
yield a true at that point in execution.

A trivial example might be as follows. Suppose an object has three
values, ``v1``, ``v2``, and ``sum``. The invariant is that the third
value is the sum of the other two. Expressed mathematically, the
invariant is ``sum = v1 + v2``. Any modification of ``v1`` or ``v2``
requires modification of ``sum``. Since that is a complex operation, it
must be monitored. Furthermore, any type of access to ``sum`` must also
be monitored to ensure that neither ``v1`` nor ``v2`` are in flux.

.. note::

   Evaluation of the invariant expression is a conceptual
   requirement and is rarely done in practice. It is valuable to
   formally define the expression during design, write it down, and
   adhere to it. It is also useful to implement the expression during
   development and test it where appropriate. The thread makes an
   absolute assertion of the expression's evaluation both on entering
   and on exiting the monitor.

Acquiring a lock is a synchronous operation. Once the lock primitive is
called, the thread returns only when it has acquired the lock. Should
another thread (or the same thread) already have the lock held, the
calling thread blocks, waiting for the situation to improve. That
blocked state is not interruptible, nor is it timed.

.. _Condition_Variables:

Condition Variables
~~~~~~~~~~~~~~~~~~~

Condition variables facilitate communication between threads. The
communication available is a semantic-free notification whose context
must be supplied by the programmer. Conditions are closely associated
with a single monitor.

The association between a condition and a monitor is established when a
condition variable is created, and the association persists for the life
of the condition variable. In addition, a static association exists
between the condition and some data within the monitor. This data is
what will be manipulated by the program under the protection of the
monitor. A thread may wait on notification of a condition that signals
changes in the state of the associated data. Other threads may notify
the condition when changes occur.

Condition variables are always monitored. The relevant operations on
conditions are always performed from within the monitor. They are used
to communicate changes in the state of the monitored data (though still
preserving the monitored invariant). Condition variables allow one or
more threads to wait for a predetermined condition to exist, and they
allow another thread to notify them when the condition occurs. Condition
variables themselves do not carry the semantics of the state change, but
simply provide a mechanism for indicating that something has changed. It
is the programmer's responsibility to associate a condition with the
state of the data.

A thread may be designed to wait for a particular situation to exist in
some monitored data. Since the nature of the situation is not an
attribute of the condition, the program must test that itself. Since
this testing involves the monitored data, it must be done from within
the monitor. The wait operation atomically exits the monitor and blocks
the calling thread in a waiting condition state. When the thread is
resumed after the wait, it will have reentered the monitor, making
operations on the data safe.

There is a subtle interaction between the thread(s) waiting on a
condition and those notifying it. The notification must take place
within a monitor--the same monitor that protects the data being
manipulated by the notifier. In pseudocode, the sequence looks like
this:

.. code::

   enter(monitor);
   ... manipulate the monitored data
   notify(condition);
   exit(monitor);

Notifications to a condition do not accumulate. Nor is it required that
any thread be waiting on a condition when the notification occurs. The
design of the code that waits on a condition must take these facts into
account. Therefore, the pseudocode for the waiting thread might look
like this:

.. code::

   enter(monitor)
   while (!expression) wait(condition);
   ... manipulate monitored data
   exit(monitor);

The need to evaluate the Boolean expression again after rescheduling
from a wait may appear unnecessary, but it is vital to the correct
execution of the program. The notification promotes a thread waiting on
a condition to a ready state. When that thread actually gets scheduled
is determined by the thread scheduler and cannot be predicted. If
multiple threads are actually processing the notifications, one or more
of them could be scheduled ahead of the one explicitly promoted by the
notification. One such thread could enter the monitor and perform the
work indicated by the notification, and exit. In this case the thread
would resume from the wait only to find that there's nothing to do.

For example, suppose the defined rule of a function is that it should
wait until there is an object available and that it should return a
reference to that object. Writing the code as follows could potentially
return a null reference, violating the invariant of the function:

.. code::

   void *dequeue()
   {
      void *db;
      enter(monitor);
      if ((db = delink()) == null)
      {
         wait(condition);
         db = delink();
      }
      exit(monitor);
      return db;
   }

The same function would be more appropriately written as follows:

.. code::

   void *dequeue()
   {
      void *db;
      enter(monitor);
      while ((db = delink()) == null)
         wait(condition);
      exit(monitor);
      return db;
   }

.. note::

   **Caution**: The semantics of :ref:`PR_WaitCondVar` assume that the
   monitor is about to be exited. This assumption implies that the
   monitored invariant must be reinstated before calling
   :ref:`PR_WaitCondVar`. Failure to do this will cause subtle but painful
   bugs.

To modify monitored data safely, a thread must be in the monitor. Since
no other thread may modify or (in most cases) even observe the protected
data from outside the monitor, the thread can safely make any
modifications needed. When the changes have been completed, the thread
notifies the condition associated with the data and exits the monitor
using :ref:`PR_NotifyCondVar`. Logically, each such notification promotes
one thread that was waiting on the condition to a ready state. An
alternate form of notification (:ref:`PR_NotifyAllCondVar`) promotes all
threads waiting on a condition to the ready state. If no threads were
waiting, the notification is a no-op.

Waiting on a condition variable is an interruptible operation. Another
thread could target the waiting thread and issue a :ref:`PR_Interrupt`,
causing a waiting thread to resume. In such cases the return from the
wait operation indicates a failure and definitively indicates that the
cause of the failure is an interrupt.

A call to :ref:`PR_WaitCondVar` may also resume because the interval
specified on the wait call has expired. However, this fact cannot be
unambiguously delivered, so no attempt is made to do so. If the logic of
a program allows for timing of waits on conditions, then the clock must
be treated as part of the monitored data and the amount of time elapsed
re-asserted when the call returns. Philosophically, timeouts should be
treated as explicit notifications, and therefore require the testing of
the monitored data upon resumption.

.. _NSPR_Sample_Code:

NSPR Sample Code
----------------

The documents linked here present two sample programs, including
detailed annotations: ``layer.html`` and ``switch.html``. In addition to
these annotated HTML versions, the same samples are available in pure
source form.
