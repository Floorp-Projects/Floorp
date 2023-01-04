PR_InitializeNetAddr
====================

Initializes or reinitializes a network address. The storage for the
network address structure is allocated by, and remains the
responsibility of, the calling client.


Syntax
------

.. code::

   #include <prnetdb.h>

   PRStatus PR_InitializeNetAddr(
     PRNetAddrValue val,
     PRUint16 port,
     PRNetAddr *addr);


Parameters
~~~~~~~~~~

The function has the following parameters:

``val``
   The value to be assigned to the IP Address portion of the network
   address. This must be ``PR_IpAddrNull``, ``PR_IpAddrAny``, or
   ``PR_IpAddrLoopback``.
``port``
   The port number to be assigned in the network address structure. The
   value is specified in host byte order.
``addr``
   A pointer to the :ref:`PRNetAddr` structure to be manipulated.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. This may occur, for example, if the
   value of val is not within the ranges defined by ``PRNetAddrValue``.
   You can retrieve the reason for the failure by calling
   :ref:`PR_GetError`.


Description
-----------

:ref:`PR_InitializeNetAddr` allows the assignment of special network
address values and the port number, while also setting the state that
indicates the version of the address being used.

The special network address values are identified by the enum
``PRNetAddrValue``:

.. code::

   typedef enum PRNetAddrValue{
     PR_IpAddrNull,
     PR_IpAddrAny,
     PR_IpAddrLoopback
   } PRNetAddrValue;

The enum has the following enumerators:

``PR_IpAddrNull``
   Do not overwrite the IP address. This allows the caller to change the
   network address' port number assignment without affecting the host
   address.
``PR_IpAddrAny``
   Assign logical ``PR_INADDR_ANY`` to IP address. This wildcard value
   is typically used to establish a socket on which to listen for
   incoming connection requests.
``PR_IpAddrLoopback``
   Assign logical ``PR_INADDR_LOOPBACK``. A client can use this value to
   connect to itself without knowing the host's network address.
