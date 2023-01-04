PR_GetInheritedFileMap
======================

Imports a :ref:`PRFileMap` previously exported by my parent process via
``PR_CreateProcess``.


Syntax
------

.. code::

   #include <prshma.h>

   NSPR_API( PRFileMap *)
   PR_GetInheritedFileMap(
     const char *shmname
   );


Parameter
~~~~~~~~~

The function has the following parameter:

``shmname``
   The name provided to :ref:`PR_ProcessAttrSetInheritableFileMap`.


Returns
~~~~~~~

Pointer to :ref:`PRFileMap` or ``NULL`` on error.


Description
-----------

:ref:`PR_GetInheritedFileMap` retrieves a PRFileMap object exported from
its parent process via ``PR_CreateProcess``.

.. note::

   **Note:** This function is not implemented.
