PR_Read
=======

Reads bytes from a file or socket.


Syntax
------

.. code::

   #include <prio.h>

   PRInt32 PR_Read(PRFileDesc *fd,
                  void *buf,
                  PRInt32 amount);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object for the file or socket.
``buf``
   A pointer to a buffer to hold the data read in. On output, the buffer
   contains the data.
``amount``
   The size of ``buf`` (in bytes).


Returns
~~~~~~~

One of the following values:

-  A positive number indicates the number of bytes actually read in.
-  The value 0 means end of file is reached or the network connection is
   closed.
-  The value -1 indicates a failure. To get the reason for the failure,
   call :ref:`PR_GetError`.


Description
-----------

The thread invoking :ref:`PR_Read` blocks until it encounters an
end-of-stream indication, some positive number of bytes (but no more
than ``amount`` bytes) are read in, or an error occurs.
