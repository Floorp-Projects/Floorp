PR_GetFileInfo64
================

Gets information about a file with a specified pathname. File size is
expressed as a 64-bit integer.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_GetFileInfo64(
     const char *fn,
     PRFileInfo64 *info);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fn``
   The pathname of the file to get information about.
``info``
   A pointer to a 64-bit file information object (see :ref:`PRFileInfo64`).
   On output, :ref:`PR_GetFileInfo64` writes information about the given
   file to the file information object.


Returns
~~~~~~~

One of the following values:

-  If the file information is successfully obtained, ``PR_SUCCESS``.
-  If the file information is not successfully obtained, ``PR_FAILURE``.


Description
-----------

:ref:`PR_GetFileInfo64` stores information about the file with the
specified pathname in the :ref:`PRFileInfo64` structure pointed to by
``info``. The file size is returned as an unsigned 64-bit integer.


See Also
--------

For the 32-bit version of this function, see :ref:`PR_GetFileInfo`. To get
equivalent information on a file that's already open, use
:ref:`PR_GetOpenFileInfo64`.
