Optimizing applications for NSPR
================================

NetScape Portable Runtime (NSPR) tries to provide a consistent level of
service across the platforms it supports. This has proven to be quite
challenging, a challenge that was met to a large degree, but there is
always room for improvement. The casual client may not encounter a need
to know the details of the shortcomings to the level described here, but
if and when clients become more sophisticated, these issues will
certainly surface.

   *This memo is by no way complete.*

Multiplatform
-------------

-  Do not call any blocking system call from a local thread. The only
   exception to this rule is the <tt>select()</tt> and <tt>poll()</tt>
   system calls on Unix, both of which NSPR has overridden to make sure
   they are aware of the NSPR local threads.
-  In the combined (MxN) model, which includes NT, IRIX (sprocs), and
   pthreads-user, the primordial thread is always a local thread.
   Therefore, if you call a blocking system call from the primordial
   thread, it is going to block more than just the primordial thread and
   the system may not function correctly. On NT, this problem is
   especially obvious because the idle thread, which is in charge of
   driving the asynch io completion port, is also blocked. Do not call
   blocking system calls from the primordial thread. Create a global
   thread and call the system call in that thread, and have the
   primordial thread join that thread.
-  NSPR uses timer signals to implement thread preemption for local
   threads on some platforms. If all the software linked into the
   application is not ported to the NSPR API, the application may fail
   because of threads being preempted during critical sections. To
   disable thread preemption call
   <tt>PR_DisableClockInterrupts()</tt>during initialization.
-  Interrupting threads (via <tt>PR_Interrupt()</tt>) on threads blocked
   in I/O functions is implemented to various degrees on different
   platforms. The UNIX based platforms all implement the function though
   there may be up to a 5 second delay in processing the request.
-  The mechanism used to implement <tt>PR_Interrupt()</tt> on the
   *pthreads* versions of NSPR is flawed. No failure attributable to the
   flaw has shown up in any tests or products - yet. The specific area
   surrounding pthread's *continuation thread* has been both observed
   and empirically proven faulty, and a correction identified.
