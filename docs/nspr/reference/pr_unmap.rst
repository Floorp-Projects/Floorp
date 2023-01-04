PR_MemUnmap
===========

Unmap a memory region that is backed by a memory-mapped file.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_MemUnmap(
     void *addr,
     PRUint32 len);


Parameters
~~~~~~~~~~

The function has the following parameters:

``addr``
   The starting address of the memory region to be unmapped.
``len``
   The length, in bytes, of the memory region.


Returns
~~~~~~~

The function returns one of the following values:

-  If the memory region is successfully unmapped, ``PR_SUCCESS``.
-  If the memory region is not successfully unmapped, ``PR_FAILURE``.
   The error code can be retrieved via :ref:`PR_GetError`.


Description
-----------

:ref:`PR_MemUnmap` removes the file mapping for the memory region
(``addr``, ``addr + len``). The parameter ``addr`` is the return value
of an earlier call to :ref:`PR_MemMap`.
