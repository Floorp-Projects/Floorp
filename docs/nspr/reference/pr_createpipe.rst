PR_CreatePipe
=============

Creates an anonymous pipe and retrieves file descriptors for the read
and write ends of the pipe.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_CreatePipe(
     PRFileDesc **readPipe,
     PRFileDesc **writePipe);


Parameters
~~~~~~~~~~

The function has the following parameters:

``readPipe``
   A pointer to a :ref:`PRFileDesc` pointer. On return, this parameter
   contains the file descriptor for the read end of the pipe.
``writePipe``
   A pointer to a :ref:`PRFileDesc` pointer. On return, this parameter
   contains the file descriptor for the write end of the pipe.


Returns
~~~~~~~

The function returns one of these values:

-  If the pipe is successfully created, ``PR_SUCCESS``.
-  If the pipe is not successfully created, ``PR_FAILURE``. The error
   code can be retrieved via :ref:`PR_GetError`.


Description
-----------

:ref:`PR_CreatePipe` creates an anonymous pipe. Data written into the write
end of the pipe can be read from the read end of the pipe. Pipes are
useful for interprocess communication between a parent and a child
process. When the pipe is no longer needed, both ends should be closed
with calls to :ref:`PR_Close`.

:ref:`PR_CreatePipe` is currently implemented on Unix, Linux, Mac OS X, and
Win32 only.
