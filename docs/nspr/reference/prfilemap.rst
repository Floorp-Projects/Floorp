PRFileMap
=========

Type returned by :ref:`PR_CreateFileMap` and passed to :ref:`PR_MemMap` and
:ref:`PR_CloseFileMap`.


Syntax
------

.. code::

   #include <prio.h>

   typedef struct PRFileMap PRFileMap;


Description
-----------

The opaque structure :ref:`PRFileMap` represents a memory-mapped file
object. Before actually mapping a file to memory, you must create a
memory-mapped file object by calling :ref:`PR_CreateFileMap`, which returns
a pointer to :ref:`PRFileMap`. Then sections of the file can be mapped into
memory by passing the :ref:`PRFileMap` pointer to :ref:`PR_MemMap`. The
memory-mapped file object is closed by passing the :ref:`PRFileMap` pointer
to :ref:`PR_CloseFileMap`.
