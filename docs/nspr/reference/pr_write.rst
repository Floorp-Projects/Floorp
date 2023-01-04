PR_Write
========

Writes a buffer of data to a file or socket.


Syntax
------

.. code::

   #include <prio.h>

   PRInt32 PR_Write(
     PRFileDesc *fd,
     const void *buf,
     PRInt32 amount);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to the :ref:`PRFileDesc` object for a file or socket.
``buf``
   A pointer to the buffer holding the data to be written.
``amount``
   The amount of data, in bytes, to be written from the buffer.


Returns
~~~~~~~

One of the following values:

-  A positive number indicates the number of bytes successfully written.
-  The value -1 indicates that the operation failed. The reason for the
   failure is obtained by calling :ref:`PR_GetError`.


Description
-----------

The thread invoking :ref:`PR_Write` blocks until all the data is written or
the write operation fails. Therefore, the return value is equal to
either ``amount`` (success) or -1 (failure). Note that if :ref:`PR_Write`
returns -1, some data (less than ``amount`` bytes) may have been written
before an error occurred.
