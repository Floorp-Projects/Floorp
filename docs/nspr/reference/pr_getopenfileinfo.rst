PR_GetOpenFileInfo
==================

Gets an open file's information. File size is expressed as a 32-bit
integer.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRStatus PR_GetOpenFileInfo(
     PRFileDesc *fd,
     PRFileInfo *info);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object for an open file.
``info``
   A pointer to a :ref:`PRFileInfo` object. On output, information about
   the given file is written into the file information object.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  If file information is successfully obtained, ``PR_SUCCESS``.
-  If file information is not successfully obtained, ``PR_FAILURE``.

.. _Description:

Description
-----------

:ref:`PR_GetOpenFileInfo` obtains the file type (normal file, directory, or
other), file size (as a 32-bit integer), and the file creation and
modification times of the open file represented by the file descriptor.

.. _See_Also:

See Also
--------

For the 64-bit version of this function, see :ref:`PR_GetOpenFileInfo64`.
To get equivalent information on a file that's not already open, use
:ref:`PR_GetFileInfo`.
