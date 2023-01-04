PR_GetFileInfo
==============

Gets information about a file with a specified pathname. File size is
expressed as a 32-bit integer.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_GetFileInfo(
     const char *fn,
     PRFileInfo *info);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fn``
   The pathname of the file to get information about.
``info``
   A pointer to a file information object (see :ref:`PRFileInfo`). On
   output, :ref:`PR_GetFileInfo` writes information about the given file to
   the file information object.


Returns
~~~~~~~

One of the following values:

-  If the file information is successfully obtained, ``PR_SUCCESS``.
-  If the file information is not successfully obtained, ``PR_FAILURE``.


Description
-----------

:ref:`PR_GetFileInfo` stores information about the file with the specified
pathname in the :ref:`PRFileInfo` structure pointed to by ``info``. The
file size is returned as an unsigned 32-bit integer.


See Also
--------

For the 64-bit version of this function, see :ref:`PR_GetFileInfo64`. To
get equivalent information on a file that's already open, use
:ref:`PR_GetOpenFileInfo`.
