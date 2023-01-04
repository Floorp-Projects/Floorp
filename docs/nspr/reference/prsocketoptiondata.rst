PRSocketOptionData
==================

Type for structure used with :ref:`PR_GetSocketOption` and
:ref:`PR_SetSocketOption` to specify options for file descriptors that
represent sockets.


Syntax
------

.. code::

   #include <prio.h>

   typedef struct PRSocketOptionData
   {
     PRSockOption option;
     union
     {
        PRUintn ip_ttl;
        PRUintn mcast_ttl;
        PRUintn tos;
        PRBool non_blocking;
        PRBool reuse_addr;
        PRBool keep_alive;
        PRBool mcast_loopback;
        PRBool no_delay;
        PRSize max_segment;
        PRSize recv_buffer_size;
        PRSize send_buffer_size;
        PRLinger linger;
        PRMcastRequest add_member;
        PRMcastRequest drop_member;
        PRNetAddr mcast_if;
     } value;
   } PRSocketOptionData;


Fields
~~~~~~

The structure has the following fields:

``ip_ttl``
   IP time-to-live.
``mcast_ttl``
   IP multicast time-to-live.
``tos``
   IP type-of-service and precedence.
``non_blocking``
   Nonblocking (network) I/O.
``reuse_addr``
   Allow local address reuse.
``keep_alive``
   Periodically test whether connection is still alive.
``mcast_loopback``
   IP multicast loopback.
``no_delay``
   Disable Nagle algorithm. Don't delay send to coalesce packets.
``max_segment``
   TCP maximum segment size.
``recv_buffer_size``
   Receive buffer size.
``send_buffer_size``
   Send buffer size.
``linger``
   Time to linger on close if data are present in socket send buffer.
``add_member``
   Join an IP multicast group.
``drop_member``
   Leave an IP multicast group.
``mcast_if``
   IP multicast interface address.


Description
~~~~~~~~~~~

:ref:`PRSocketOptionData` is a name-value pair for a socket option. The
``option`` field (of enumeration type :ref:`PRSockOption`) specifies the
name of the socket option, and the ``value`` field (a union of all
possible values) specifies the value of the option.
