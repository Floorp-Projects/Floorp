PR_NewTCPSocket
===============

Creates a new IPv4 TCP socket.


Syntax
------

.. code::

   #include <prio.h>

   PRFileDesc* PR_NewTCPSocket(void);


Returns
~~~~~~~

The function returns one of the following values:

-  Upon successful completion, a pointer to the :ref:`PRFileDesc` object
   created for the newly opened IPv4 TCP socket.
-  If the creation of a new TCP socket failed, ``NULL``.


Description
-----------

TCP (Transmission Control Protocol) is a connection-oriented, reliable
byte-stream protocol of the TCP/IP protocol suite. :ref:`PR_NewTCPSocket`
creates a new IPv4 TCP socket. A TCP connection is established by a
passive socket (the server) accepting a connection setup request from an
active socket (the client). Typically, the server binds its socket to a
well-known port with :ref:`PR_Bind`, calls :ref:`PR_Listen` to start listening
for connection setup requests, and calls :ref:`PR_Accept` to accept a
connection. The client makes a connection request using :ref:`PR_Connect`.

After a connection is established, the client and server may send and
receive data between each other. To receive data, one can call
:ref:`PR_Read` or :ref:`PR_Recv`. To send data, one can call :ref:`PR_Write`,
:ref:`PR_Writev`, :ref:`PR_Send`, or :ref:`PR_TransmitFile`. :ref:`PR_AcceptRead` is
suitable for use by the server to accept a new client connection and
read the client's first request in one function call.

A TCP connection can be shut down by :ref:`PR_Shutdown`, and the sockets
should be closed by :ref:`PR_Close`.


See Also
--------

:ref:`PR_NewTCPSocket` is deprecated because it is hardcoded to create an
IPv4 TCP socket. New code should use :ref:`PR_OpenTCPSocket` instead, which
allows the address family (IPv4 or IPv6) of the new TCP socket to be
specified.
