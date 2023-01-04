PR_MemMap
=========

Maps a section of a file to memory.


Syntax
------

.. code::

   #include <prio.h>

   void* PR_MemMap(
     PRFileMap *fmap,
     PRInt64 offset,
     PRUint32 len);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fmap``
   A pointer to the file-mapping object representing the file to be
   memory-mapped.
``offset``
   The starting offset of the section of file to be mapped. The offset
   must be aligned to whole pages.
``len``
   Length of the section of the file to be mapped.


Returns
~~~~~~~

The starting address of the memory region to which the section of file
is mapped. Returns ``NULL`` on error.


Description
-----------

:ref:`PR_MemMap` maps a section of the file represented by the file mapping
``fmap`` to memory. The section of the file starts at ``offset`` and has
the length ``len``.

When the file-mapping memory region is no longer needed, it should be
unmapped with a call to :ref:`PR_MemUnmap`.
