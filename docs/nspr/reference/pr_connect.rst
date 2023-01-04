PR_Connect
==========

Initiates a connection on a specified socket.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_Connect(
     PRFileDesc *fd,
     const PRNetAddr *addr,
     PRIntervalTime timeout);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing a socket.
``addr``
   A pointer to the address of the peer to which the socket is to be
   connected.
``timeout``
   A value of type :ref:`PRIntervalTime` specifying the time limit for
   completion of the connect operation.


Returns
~~~~~~~

The function returns one of the following values:

-  Upon successful completion of connection setup, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. Further information can be obtained
   by calling :ref:`PR_GetError`.


Description
-----------

:ref:`PR_Connect` is usually invoked on a TCP socket, but it may also be
invoked on a UDP socket. Both cases are discussed here.

If the socket is a TCP socket, :ref:`PR_Connect` establishes a TCP
connection to the peer. If the socket is not bound, it will be bound to
an arbitrary local address.

:ref:`PR_Connect` blocks until either the connection is successfully
established or an error occurs. The function uses the lesser of the
provided timeout and the OS's connect timeout. In particular, if you
specify ``PR_INTERVAL_NO_TIMEOUT`` as the timeout, the OS's connection
time limit will be used.

If the socket is a UDP socket, there is no connection setup to speak of,
since UDP is connectionless. If :ref:`PR_Connect` is invoked on a UDP
socket, it has an overloaded meaning: :ref:`PR_Connect` merely saves the
specified address as the default peer address for the socket, so that
subsequently one can send and receive datagrams from the socket using
:ref:`PR_Send` and :ref:`PR_Recv` instead of the usual :ref:`PR_SendTo` and
:ref:`PR_RecvFrom`.
