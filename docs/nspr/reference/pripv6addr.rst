PRIPv6Addr
==========

Type used in the ``ipv6.ip`` field of the :ref:`PRNetAddr` structure.


Syntax
------

.. code::

   #include <prio.h>

   #if defined(_PR_INET6)
      typedef struct in6_addr PRIPv6Addr;
   #endif /* defined(_PR_INET6) */


Description
-----------

PRIPv6Addr represents a 128-bit IPv6 address. It is equivalent to struct
``in6_addr`` in the Berkeley socket interface. :ref:`PRIPv6Addr` is always
manipulated as a byte array. Unlike the IPv4 address (a 4-byte unsigned
integer) or the port number (a 2-byte unsigned integer), it has no
network or host byte order.
