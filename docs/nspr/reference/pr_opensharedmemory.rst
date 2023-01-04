PR_OpenSharedMemory
===================

Opens an existing shared memory segment or, if one with the specified
name doesn't exist, creates a new one.


Syntax
------

.. code::

   #include <prshm.h>

   NSPR_API( PRSharedMemory * )
     PR_OpenSharedMemory(
        const char             *name,
        PRSize             size,
        PRIntn             flags,
        PRIntn             mode
   );

   /* Define values for PR_OpenShareMemory(...,create) */
   #define PR_SHM_CREATE 0x1  /* create if not exist */
   #define PR_SHM_EXCL   0x2  /* fail if already exists */


Parameters
~~~~~~~~~~

The function has the following parameters:

name
   The name of the shared memory segment.
size
   The size of the shared memory segment.
flags
   Options for creating the shared memory.
mode
   Same as passed to :ref:`PR_Open`.


Returns
~~~~~~~

Pointer to opaque structure ``PRSharedMemory``, or ``NULL`` if an error
occurs. Retrieve the reason for the failure by calling :ref:`PR_GetError`
and :ref:`PR_GetOSError`.


Description
-----------

:ref:`PR_OpenSharedMemory` creates a new shared memory segment or
associates a previously created memory segment with the specified name.
When parameter ``create`` is (``PR_SHM_EXCL`` \| ``PR_SHM_CREATE``) and
the shared memory already exists, the function returns ``NULL`` with the
error set to ``PR_FILE_EXISTS_ERROR``.

When parameter ``create`` is ``PR_SHM_CREATE`` and the shared memory
already exists, a handle to that memory segment is returned. If the
segment does not exist, it is created and a pointer to the related
``PRSharedMemory`` structure is returned.

When parameter ``create`` is 0, and the shared memory exists, a pointer
to a ``PRSharedMemory`` structure is returned. If the shared memory does
not exist, ``NULL`` is returned with the error set to
``PR_FILE_NOT_FOUND_ERROR``.
