PR_DetachSharedMemory
=====================

Unmaps a shared memory segment identified by name.


Syntax
------

.. code::

   #include <prshm.h>

   NSPR_API( PRStatus )
     PR_DetachSharedMemory(
        PRSharedMemory *shm,
        void  *addr
   );


Parameters
~~~~~~~~~~

The function has these parameters:

shm
   The handle returned from :ref:`PR_OpenSharedMemory`.
addr
   The address to which the shared memory segment is mapped.


Returns
~~~~~~~

:ref:`PRStatus`.
