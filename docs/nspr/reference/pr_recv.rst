PR_Recv
=======

Receives bytes from a connected socket.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prio.h>

   PRInt32 PR_Recv(
     PRFileDesc *fd,
     void *buf,
     PRInt32 amount,
     PRIntn flags,
     PRIntervalTime timeout);

.. _Parameters:

Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing a socket.
``buf``
   A pointer to a buffer to hold the data received.
``amount``
   The size of ``buf`` (in bytes).
``flags``
   Must be zero or ``PR_MSG_PEEK``.
``timeout``
   A value of type :ref:`PRIntervalTime` specifying the time limit for
   completion of the receive operation.

.. _Returns:

Returns
~~~~~~~

The function returns one of the following values:

-  A positive number indicates the number of bytes actually received.
-  The value 0 means the network connection is closed.
-  The value -1 indicates a failure. The reason for the failure can be
   obtained by calling :ref:`PR_GetError`.

.. _Description:

Description
-----------

:ref:`PR_Recv` blocks until some positive number of bytes are transferred,
a timeout occurs, or an error occurs. No more than ``amount`` bytes will
be transferred.
