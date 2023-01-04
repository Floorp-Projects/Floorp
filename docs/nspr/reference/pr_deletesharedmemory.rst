PR_DeleteSharedMemory
=====================

Deletes a shared memory segment identified by name.


Syntax
------

.. code::

   #include <prshm.h>

   NSPR_API( PRStatus )
     PR_DeleteSharedMemory(
        const char *name
   );


Parameter
~~~~~~~~~

The function has these parameter:

shm
   The handle returned from :ref:`PR_OpenSharedMemory`.


Returns
~~~~~~~

:ref:`PRStatus`.
