PR_CreateFileMap
================

Creates a file mapping object.


Syntax
------

.. code::

   #include <prio.h>

   PRFileMap* PR_CreateFileMap(
     PRFileDesc *fd,
     PRInt64 size,
     PRFileMapProtect prot);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing the file that is to
   be mapped to memory.
``size``
   Size of the file specified by ``fd``.
``prot``
   Protection option for read and write accesses of a file mapping. This
   parameter consists of one of the following options:

    - :ref:`PR_PROT_READONLY`. Read-only access.
    - :ref:`PR_PROT_READWRITE`. Readable, and write is shared.
    - :ref:`PR_PROT_WRITECOPY`. Readable, and write is private
      (copy-on-write).


Returns
~~~~~~~

-  If successful, a file mapping of type :ref:`PRFileMap`.
-  If unsuccessful, ``NULL``.


Description
-----------

The ``PRFileMapProtect`` enumeration used in the ``prot`` parameter is
defined as follows:

.. code::

   typedef enum PRFileMapProtect {
     PR_PROT_READONLY,
     PR_PROT_READWRITE,
     PR_PROT_WRITECOPY
   } PRFileMapProtect;

:ref:`PR_CreateFileMap` only prepares for the mapping a file to memory. The
returned file-mapping object must be passed to :ref:`PR_MemMap` to actually
map a section of the file to memory.

The file-mapping object should be closed with a :ref:`PR_CloseFileMap` call
when it is no longer needed.
