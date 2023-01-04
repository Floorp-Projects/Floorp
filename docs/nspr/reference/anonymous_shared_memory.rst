Anonymous Shared Memory
=======================

This chapter describes the NSPR API for anonymous shared memory.

-  `Anonymous Memory Protocol <#Anonymous_Memory_Protocol>`__
-  `Anonymous Shared Memory
   Functions <#Anonymous_Shared_Memory_Functions>`__

.. _Anonymous_Memory_Protocol:

Anonymous Memory Protocol
-------------------------

NSPR provides an anonymous shared memory based on NSPR's :ref:`PRFileMap`
type. The anonymous file-mapped shared memory provides an inheritable
shared memory, as in: the child process inherits the shared memory.
Compare the file-mapped anonymous shared memory to to a named shared
memory described in prshm.h. The intent is to provide a shared memory
that is accessible only by parent and child processes. ... It's a
security thing.

Depending on the underlying platform, the file-mapped shared memory may
be backed by a file. ... surprise! ... On some platforms, no real file
backs the shared memory. On platforms where the shared memory is backed
by a file, the file's name in the filesystem is visible to other
processes for only the duration of the creation of the file, hopefully a
very short time. This restricts processes that do not inherit the shared
memory from opening the file and reading or writing its contents.
Further, when all processes using an anonymous shared memory terminate,
the backing file is deleted. ... If you are not paranoid, you're not
paying attention.

The file-mapped shared memory requires a protocol for the parent process
and child process to share the memory. NSPR provides two protocols. Use
one or the other; don't mix and match.

In the first protocol, the job of passing the inheritable shared memory
is done via helper-functions with PR_CreateProcess. In the second
protocol, the parent process is responsible for creating the child
process; the parent and child are mutually responsible for passing a
``FileMap`` string. NSPR provides helper functions for extracting data
from the :ref:`PRFileMap` object. ... See the examples below.

Both sides should adhere strictly to the protocol for proper operation.
The pseudo-code below shows the use of a file-mapped shared memory by a
parent and child processes. In the examples, the server creates the
file-mapped shared memory, the client attaches to it.

.. _First_protocol:

First protocol
~~~~~~~~~~~~~~

**Server:**

.. code::

   fm = PR_OpenAnonFileMap(dirName, size, FilemapProt);
   addr = PR_MemMap(fm);
   attr = PR_NewProcessAttr();
   PR_ProcessAttrSetInheritableFileMap( attr, fm, shmname );
   PR_CreateProcess(Client);
   PR_DestroyProcessAttr(attr);
   ... yadda ...
   PR_MemUnmap( addr );
   PR_CloseFileMap(fm);

**Client:**

.. code::

   ... started by server via PR_CreateProcess()
   fm = PR_GetInheritedFileMap( shmname );
   addr = PR_MemMap(fm);
   ... yadda ...
   PR_MemUnmap(addr);
   PR_CloseFileMap(fm);

.. _Second_protocol:

Second protocol
~~~~~~~~~~~~~~~

**Server:**

.. code::

   fm = PR_OpenAnonFileMap(dirName, size, FilemapProt);
   fmstring = PR_ExportFileMapAsString( fm );
   addr = PR_MemMap(fm);
   ... application specific technique to pass fmstring to child
   ... yadda ... Server uses his own magic to create child
   PR_MemUnmap( addr );
   PR_CloseFileMap(fm);

**Client:**

.. code::

   ... started by server via his own magic
   ... application specific technique to find fmstring from parent
   fm = PR_ImportFileMapFromString( fmstring )
   addr = PR_MemMap(fm);
   ... yadda ...
   PR_MemUnmap(addr);
   PR_CloseFileMap(fm);

.. _Anonymous_Shared_Memory_Functions:

Anonymous Shared Memory Functions
---------------------------------

-  :ref:`PR_OpenAnonFileMap`
-  :ref:`PR_ProcessAttrSetInheritableFileMap`
-  :ref:`PR_GetInheritedFileMap`
-  :ref:`PR_ExportFileMapAsString`
-  :ref:`PR_ImportFileMapFromString`
