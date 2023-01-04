PR_ProcessAttrSetInheritableFileMap
===================================

Prepare filemap for export to my children processes via
``PR_CreateProcess``.


Syntax
------

.. code::

   #include <prshma.h>

   NSPR_API(PRStatus)
   PR_ProcessAttrSetInheritableFileMap(
     PRProcessAttr   *attr,
     PRFileMap       *fm,
     const char      *shmname
   );


Parameters
~~~~~~~~~~

The function has the following parameters:

``attr``
   Pointer to a PRProcessAttr structure used to pass data to
   PR_CreateProcess.
``fm``
   Pointer to a PRFileMap structure to be passed to the child process.
``shmname``
   Pointer to the name for the PRFileMap; used by child.


Returns
~~~~~~~

:ref:`PRStatus`


Description
~~~~~~~~~~~

:ref:`PR_ProcessAttrSetInheritableFileMap` connects the :ref:`PRFileMap` to
:ref:`PRProcessAttr` with ``shmname``. A subsequent call to
``PR_CreateProcess`` makes the :ref:`PRFileMap` importable by the child
process.

.. note::

   **Note:** This function is not implemented.
