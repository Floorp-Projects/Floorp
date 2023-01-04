PR_Rename
=========

Renames a file.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_Rename(
     const char *from,
     const char *to);


Parameters
~~~~~~~~~~

The function has the following parameters:

``from``
   The old name of the file to be renamed.
``to``
   The new name of the file.


Returns
~~~~~~~

One of the following values:

-  If file is successfully renamed, ``PR_SUCCESS``.
-  If file is not successfully renamed, ``PR_FAILURE``.


Description
-----------

:ref:`PR_Rename` renames a file from its old name (``from``) to a new name
(``to``). If a file with the new name already exists, :ref:`PR_Rename`
fails with the error code ``PR_FILE_EXISTS_ERROR``. In this case,
:ref:`PR_Rename` does not overwrite the existing filename.
