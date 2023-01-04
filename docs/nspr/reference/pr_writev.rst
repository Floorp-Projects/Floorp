PR_Writev
=========

Writes data to a socket from multiple buffers.


Syntax
------

.. code::

   #include <prio.h>

   PRInt32 PR_Writev(
     PRFileDesc *fd,
     PRIOVec *iov,
     PRInt32 size,
     PRIntervalTime timeout);

   #define PR_MAX_IOVECTOR_SIZE 16


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object for a socket.
``iov``
   An array of ``PRIOVec`` structures that describe the buffers to write
   from.
``size``
   Number of ``PRIOVec`` structures in the ``iov`` array. The value of
   this parameter must not be greater than ``PR_MAX_IOVECTOR_SIZE``. If
   it is, the function will fail and the error will be set to
   ``PR_BUFFER_OVERFLOW_ERROR``.
``timeout``
   A value of type :ref:`PRIntervalTime` describing the time limit for
   completion of the entire write operation.


Returns
~~~~~~~

One of the following values:

-  A positive number indicates the number of bytes successfully written.
-  The value -1 indicates that the operation failed. The reason for the
   failure can be obtained by calling :ref:`PR_GetError`.


Description
-----------

The thread calling :ref:`PR_Writev` blocks until all the data is written or
the write operation fails. Therefore, the return value is equal to
either the sum of all the buffer lengths (on success) or -1 (on
failure). Note that if :ref:`PR_Writev` returns -1, part of the data may
have been written before an error occurred. If the timeout parameter is
not ``PR_INTERVAL_NO_TIMEOUT`` and all the data cannot be written in the
specified interval, :ref:`PR_Writev` returns -1 with the error code
``PR_IO_TIMEOUT_ERROR``.

This is the type definition for ``PRIOVec``:

.. code::

   typedef struct PRIOVec {
     char *iov_base;
     int iov_len;
   } PRIOVec;

The ``PRIOVec`` structure has the following fields:

``iov_base``
   A pointer to the beginning of the buffer.
``iov_len``
   The size of the buffer.
