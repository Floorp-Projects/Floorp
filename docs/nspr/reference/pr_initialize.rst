PR_Initialize
=============

Provides an alternate form of explicit initialization. In addition to
establishing the sequence of operations, :ref:`PR_Initialize` implicitly
calls :ref:`PR_Cleanup` on exiting the primordial function.


Syntax
------

.. code::

   #include <prinit.h>

   PRIntn PR_Initialize(
     PRPrimordialFn prmain,
     PRIntn argc,
     char **argv,
     PRUintn maxPTDs);


Parameters
~~~~~~~~~~

:ref:`PR_Initialize` has the following parameters:

``prmain``
   The function that becomes the primordial thread's root function.
   Returning from prmain leads to termination of the process.
``argc``
   The length of the argument vector, whether passed in from the host's
   program-launching facility or fabricated by the actual main program.
   This approach conforms to standard C programming practice.
``argv``
   The base address of an array of strings that compromise the program's
   argument vector. This approach conforms to standard C programming
   practice.
``maxPTDs``
   This parameter is ignored.


Returns
~~~~~~~

The value returned from the root function, ``prmain``.


Description
-----------

:ref:`PR_Initialize` initializes the NSPR runtime and places NSPR between
the caller and the runtime library. This allows ``main`` to be treated
like any other function, signaling its completion by returning and
allowing the runtime to coordinate the completion of the other threads
of the runtime.

:ref:`PR_Initialize` does not return to its caller until all user threads
have terminated.

The priority of the main (or primordial) thread is
``PR_PRIORITY_NORMAL``. The thread may adjust its own priority by using
:ref:`PR_SetThreadPriority`.
