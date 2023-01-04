PR_OpenUDPSocket
================

Creates a new UDP socket of the specified address family.


Syntax
------

.. code::

   #include <prio.h>

   PRFileDesc* PR_OpenUDPSocket(PRIntn af);


Parameters
~~~~~~~~~~

The function has the following parameters:

``af``
   The address family of the new UDP socket. Can be ``PR_AF_INET``
   (IPv4), ``PR_AF_INET6`` (IPv6), or ``PR_AF_LOCAL`` (Unix domain,
   supported on POSIX systems only).


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

:ref:`PR_OpenUDPSocket` creates a new UDP socket of the address family
``af``. The socket may be bound to a well-known port number with
:ref:`PR_Bind`. Datagrams can be sent with :ref:`PR_SendTo` and received with
:ref:`PR_RecvFrom`. When the socket is no longer needed, it should be
closed with a call to :ref:`PR_Close`.
