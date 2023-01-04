PR_Seek64
=========

Moves the current read-write file pointer by an offset expressed as a
64-bit integer.


Syntax
------

.. code::

   #include <prio.h>

   PRInt64 PR_Seek64(
     PRFileDesc *fd,
     PRInt64 offset,
     PRSeekWhence whence);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object.
``offset``
   A value, in bytes, used with the whence parameter to set the file
   pointer. A negative value causes seeking in the reverse direction.
``whence``
   A value of type :ref:`PRSeekWhence` that specifies how to interpret the
   ``offset`` parameter in setting the file pointer associated with the
   fd parameter. The value for the ``whence`` parameter can be one of
   the following:

    - :ref:`PR_SEEK_SET`. Sets the file pointer to the value of the
      ``offset`` parameter.
    - :ref:`PR_SEEK_CUR`. Sets the file pointer to its current location
      plus the value of the ``offset`` parameter.
    - :ref:`PR_SEEK_END`. Sets the file pointer to the size of the file
      plus the value of the ``offset`` parameter.


Returns
~~~~~~~

The function returns one of the following values:

-  If the function completes successfully, it returns the resulting file
   pointer location, measured in bytes from the beginning of the file.
-  If the function fails, the file pointer remains unchanged and the
   function returns -1. The error code can then be retrieved with
   :ref:`PR_GetError`.


Description
-----------

This is the idiom for obtaining the current location (expressed as a
64-bit integer) of the file pointer for the file descriptor ``fd``:

``PR_Seek64(fd, 0, PR_SEEK_CUR)``

If the operating system can handle only a 32-bit file offset,
:ref:`PR_Seek64` may fail with the error code ``PR_FILE_TOO_BIG_ERROR`` if
the ``offset`` parameter is out of the range of a 32-bit integer.


See Also
--------

:ref:`PR_Seek`
