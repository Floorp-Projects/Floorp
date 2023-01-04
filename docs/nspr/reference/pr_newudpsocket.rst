PR_NewUDPSocket
===============

Creates a new UDP socket.


Syntax
------

.. code::

   #include <prio.h>

   PRFileDesc* PR_NewUDPSocket(void);


Returns
~~~~~~~

The function returns one of the following values:

-  Upon successful completion, a pointer to the :ref:`PRFileDesc` object
   created for the newly opened UDP socket.
-  If the creation of a new UDP socket failed, ``NULL``.


Description
-----------

UDP (User Datagram Protocol) is a connectionless, unreliable datagram
protocol of the TCP/IP protocol suite. UDP datagrams may be lost or
delivered in duplicates or out of sequence.

:ref:`PR_NewUDPSocket` creates a new UDP socket. The socket may be bound to
a well-known port number with :ref:`PR_Bind`. Datagrams can be sent with
:ref:`PR_SendTo` and received with :ref:`PR_RecvFrom`. When the socket is no
longer needed, it should be closed with a call to :ref:`PR_Close`.


See Also
--------

:ref:`PR_NewUDPSocket` is deprecated because it is hardcoded to create an
IPv4 UDP socket. New code should use :ref:`PR_OpenUDPSocket` instead, which
allows the address family (IPv4 or IPv6) of the new UDP socket to be
specified.
