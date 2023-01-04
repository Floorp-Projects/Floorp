PR_AttachSharedMemory
=====================

Attaches a memory segment previously opened with :ref:`PR_OpenSharedMemory`
and maps it into the process memory space.


Syntax
------

.. code::

   #include <prshm.h>

.. code::

   NSPR_API( void * )
     PR_AttachSharedMemory(
        PRSharedMemory *shm,
        PRIntn flags
   );

   /* Define values for PR_AttachSharedMemory(...,flags) */
   #define PR_SHM_READONLY 0x01


Parameters
~~~~~~~~~~

The function has these parameters:

shm
   The handle returned from :ref:`PR_OpenSharedMemory`.
flags
   Options for mapping the shared memory. ``PR_SHM_READONLY`` causes the
   memory to be attached read-only.


Returns
~~~~~~~

Address where shared memory is mapped, or ``NULL`` if an error occurs.
Retrieve the reason for the failure by calling :ref:`PR_GetError` and
:ref:`PR_GetOSError`.
