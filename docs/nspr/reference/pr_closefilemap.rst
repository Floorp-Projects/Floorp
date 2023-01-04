PR_CloseFileMap
===============

Closes a file mapping.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_CloseFileMap(PRFileMap *fmap);


Parameter
~~~~~~~~~

The function has the following parameter:

``fmap``
   The file mapping to be closed.


Returns
~~~~~~~

The function returns one of the following values:

-  If the memory region is successfully unmapped, ``PR_SUCCESS``.
-  If the memory region is not successfully unmapped, ``PR_FAILURE``.
   The error code can be retrieved via :ref:`PR_GetError`.


Description
-----------

When a file mapping created with a call to :ref:`PR_CreateFileMap` is no
longer needed, it should be closed with a call to :ref:`PR_CloseFileMap`.
