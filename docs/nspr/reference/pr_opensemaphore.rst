PR_OpenSemaphore
================

Creates or opens a named semaphore with the specified name.


Syntax
------

.. code::

   #include <pripcsem.h>

   #define PR_SEM_CREATE 0x1 /* create if not exist */

   #define PR_SEM_EXCL 0x2 /* fail if already exists */

   NSPR_API(PRSem *) PR_OpenSemaphore(
     const char *name,
     PRIntn flags,
     PRIntn mode,
     PRUintn value
   );


Parameters
~~~~~~~~~~

The function has the following parameters:

``name``
   The name to be given the semaphore.
``flags``
   How to create or open the semaphore.
``mode``
   Unix style file mode to be used when creating the semaphore.
``value``
   The initial value assigned to the semaphore.


Returns
~~~~~~~

A pointer to a PRSem structure or ``NULL/code> on error.``


Description
~~~~~~~~~~~

If the named semaphore doesn't exist and the ``PR_SEM_CREATE`` flag is
specified, the named semaphore is created. The created semaphore needs
to be removed from the system with a :ref:`PR_DeleteSemaphore` call.

If ``PR_SEM_CREATE`` is specified, the third argument is the access
permission bits of the new semaphore (same interpretation as the mode
argument to :ref:`PR_Open`) and the fourth argument is the initial value of
the new semaphore. ``If PR_SEM_CREATE`` is not specified, the third and
fourth arguments are ignored.
