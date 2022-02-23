The chapter describes the NSPR API for named shared memory. Shared
memory allows multiple processes to access one or more common shared
memory regions, using it as an interprocess communication channel. The
NSPR shared memory API provides a cross-platform named shared-memory
interface that is modeled on similar constructs in the Unix and Windows
operating systems.

-  `Shared Memory Protocol <#Shared_Memory_Protocol>`__
-  `Named Shared Memory Functions <#Named_Shared_Memory_Functions>`__

.. _Shared_Memory_Protocol:

Shared Memory Protocol
----------------------

.. _Using_Named_Shared_Memory_Functions:

Using Named Shared Memory Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:ref:`PR_OpenSharedMemory` creates the shared memory segment, if it does
not already exist, or opens a connection with the existing shared memory
segment if it already exists.

:ref:`PR_AttachSharedMemory` should be called following
:ref:`PR_OpenSharedMemory` to map the memory segment to an address in the
application's address space. :ref:`PR_AttachSharedMemory` may also be
called to remap a shared memory segment after detaching the same
``PRSharedMemory`` object. Be sure to detach it when you're finished.

:ref:`PR_DetachSharedMemory` should be called to unmap the shared memory
segment from the application's address space.

:ref:`PR_CloseSharedMemory` should be called when no further use of the
``PRSharedMemory`` object is required within a process. Following a call
to :ref:`PR_CloseSharedMemory`, the ``PRSharedMemory`` object is invalid
and cannot be reused.

:ref:`PR_DeleteSharedMemory` should be called before process termination.
After you call :ref:`PR_DeleteSharedMemory`, any further use of the shared
memory associated with the name may cause unpredictable results.

Filenames
~~~~~~~~~

The name passed to :ref:`PR_OpenSharedMemory` should be a valid filename
for a Unix platform. :ref:`PR_OpenSharedMemory` creates file using the name
passed in. Some platforms may mangle the name before creating the file
and the shared memory. The Unix implementation may use SysV IPC shared
memory, Posix shared memory, or memory mapped files; the filename may be
used to define the namespace. On Windows, the name is significant, but
there is no file associated with the name.

No assumptions about the persistence of data in the named file should be
made. Depending on platform, the shared memory may be mapped onto system
paging space and be discarded at process termination.

All names provided to :ref:`PR_OpenSharedMemory` should be valid filename
syntax or name syntax for shared memory for the target platform.
Referenced directories should have permissions appropriate for writing.

.. _Limits_on_Shared_Memory_Resources:

Limits on Shared Memory Resources
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Different platforms have limits on both the number and size of shared
memory resources. The default system limits on some platforms may be
smaller than your requirements. These limits may be adjusted on some
platforms either via boot-time options or by setting the size of the
system paging space to accommodate more and/or larger shared memory
segment(s).

.. _Security_Considerations:

Security Considerations
~~~~~~~~~~~~~~~~~~~~~~~

On Unix platforms, depending on implementation, contents of the backing
store for the shared memory can be exposed via the file system. Set
permissions and or access controls at create and attach time to ensure
you get the desired security.

On Windows platforms, no special security measures are provided.

.. _Named_Shared_Memory_Functions:

Named Shared Memory Functions
-----------------------------

 - :ref:`PR_OpenSharedMemory`
 - :ref:`PR_AttachSharedMemory`
 - :ref:`PR_DetachSharedMemory`
 - :ref:`PR_CloseSharedMemory`
 - :ref:`PR_DeleteSharedMemory`
