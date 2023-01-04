PR_ConvertIPv4AddrToIPv6
========================

Converts an IPv4 address into an (IPv4-mapped) IPv6 address.


Syntax
~~~~~~

.. code::

   #include <prnetdb.h>

   void PR_ConvertIPv4AddrToIPv6(
     PRUint32 v4addr,
     PRIPv6Addr *v6addr
   );


Parameters
~~~~~~~~~~

The function has the following parameters:

``v4addr``
   The IPv4 address to convert into an IPv4-mapped IPv6 address. This
   must be specified in network byte order.
``v6addr``
   A pointer to a buffer, allocated by the caller, that is filled in
   with the IPv6 address on return.
