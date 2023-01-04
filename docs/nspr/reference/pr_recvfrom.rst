PR_RecvFrom
===========

Receives bytes from a socket and stores the sending peer's address.


Syntax
------

.. code::

   #include <prio.h>

   PRInt32 PR_RecvFrom(
     PRFileDesc *fd,
     void *buf,
     PRInt32 amount,
     PRIntn flags,
     PRNetAddr *addr,
     PRIntervalTime timeout);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing a socket.
``buf``
   A pointer to a buffer containing the data received.
``amount``
   The size of ``buf`` (in bytes).
``flags``
   This obsolete parameter must always be zero.
``addr``
   A pointer to the :ref:`PRNetAddr` object that will be filled in with the
   address of the sending peer on return.
``timeout``
   A value of type :ref:`PRIntervalTime` specifying the time limit for
   completion of the receive operation.


Returns
~~~~~~~

The function returns one of the following values:

-  A positive number indicates the number of bytes actually received.
-  The value 0 means the network connection is closed.
-  The value -1 indicates a failure. The reason for the failure can be
   obtained by calling :ref:`PR_GetError`.


Description
-----------

:ref:`PR_RecvFrom` receives up to a specified number of bytes from socket,
which may or may not be connected. The operation blocks until one or
more bytes are transferred, a timeout has occurred, or there is an
error. No more than ``amount`` bytes will be transferred.
:ref:`PR_RecvFrom` is usually used with a UDP socket.
