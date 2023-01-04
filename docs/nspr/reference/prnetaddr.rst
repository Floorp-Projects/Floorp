PRNetAddr
=========

Type used with `Socket Manipulation
Functions <Socket_Manipulation_Functions>`__ to specify a network
address.


Syntax
------

.. code::

   #include <prio.h>

   union PRNetAddr {
     struct {
        PRUint16 family;
        char data[14];
     } raw;
     struct {
        PRUint16 family;
        PRUint16 port;
        PRUint32 ip;
        char pad[8];
     } inet;
   #if defined(_PR_INET6)
     struct {
        PRUint16 family;
        PRUint16 port;
        PRUint32 flowinfo;
        PRIPv6Addr ip;
     } ipv6;
   #endif /* defined(_PR_INET6) */
   };

   typedef union PRNetAddr PRNetAddr;


Fields
~~~~~~

The structure has the following fields:

``family``
   Address family: ``PR_AF_INET|PR_AF_INET6`` for ``raw.family``,
   ``PR_AF_INET`` for ``inet.family``, ``PR_AF_INET6`` for
   ``ipv6.family``.
``data``
   Raw address data.
``port``
   Port number of TCP or UDP, in network byte order.
``ip``
   The actual 32 (for ``inet.ip``) or 128 (for ``ipv6.ip``) bits of IP
   address. The ``inet.ip`` field is in network byte order.
``pad``
   Unused.
``flowinfo``
   Routing information.


Description
-----------

The union :ref:`PRNetAddr` represents a network address. NSPR supports only
the Internet address family. By default, NSPR is built to support only
IPv4, but it's possible to build the NSPR library to support both IPv4
and IPv6. Therefore, the ``family`` field can be ``PR_AF_INET`` only for
default NSPR, and can also be ``PR_AF_INET6`` if the binary supports
``IPv6``.

:ref:`PRNetAddr` is binary-compatible with the socket address structures in
the familiar Berkeley socket interface, although this fact should not be
relied upon. The raw member of the union is equivalent to
``struct sockaddr``, the ``inet`` member is equivalent to
``struct sockaddr_in``, and if the binary is built with ``IPv6``
support, the ``ipv6`` member is equivalent to ``struct sockaddr_in6``.
(Note that :ref:`PRNetAddr` does not have the ``length`` field that is
present in ``struct sockaddr_in`` on some Unix platforms.)

The macros ``PR_AF_INET``, ``PR_AF_INET6``, ``PR_INADDR_ANY``,
``PR_INADDR_LOOPBACK`` are defined if ``prio.h`` is included.
``PR_INADDR_ANY`` and ``PR_INADDR_LOOPBACK`` are special ``IPv4``
addresses in host byte order, so they must be converted to network byte
order before being assigned to the ``inet.ip`` field.
