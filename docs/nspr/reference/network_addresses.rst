This chapter describes the NSPR types and functions used to manipulate
network addresses.

-  `Network Address Types and
   Constants <#Network_Address_Types_and_Constants>`__
-  `Network Address Functions <#Network_Address_Functions>`__

The API described in this chapter recognizes the emergence of Internet
Protocol Version 6 (IPv6). To facilitate the transition to IPv6, it is
recommended that clients treat all structures containing network
addresses as transparent objects and use the functions documented here
to manipulate the information.

If used consistently, this API also eliminates the need to deal with the
byte ordering of network addresses. Typically, the only numeric
declarations required are the well-known port numbers that are part of
the :ref:`PRNetAddr` structure.

.. _Network_Address_Types_and_Constants:

Network Address Types and Constants
-----------------------------------

 - :ref:`PRHostEnt`
 - :ref:`PRProtoEnt`
 - :ref:`PR_NETDB_BUF_SIZE`

.. _Network_Address_Functions:

Network address functions
-------------------------

.. _Initializing_a_Network_Address:

Initializing a network address
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:ref:`PR_InitializeNetAddr` facilitates the use of :ref:`PRNetAddr`, the basic
network address structure, in a polymorphic manner. By using these
functions with other network address functions, clients can support
either version 4 or version 6 of the Internet Protocol transparently.

All NSPR functions that require `PRNetAddr <PRNetAddr>`__ as an argument
accept either an IPv4 or IPv6 version of the address.

 - :ref:`PR_InitializeNetAddr`

.. _Converting_Between_a_String_and_a_Network_Address:

Converting between a string and a network address
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_StringToNetAddr`
 - :ref:`PR_NetAddrToString`

.. _Converting_address_formats:

Converting address formats
~~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_ConvertIPv4AddrToIPv6`

.. _Getting_Host_Names_and_Addresses:

Getting host names and addresses
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_GetHostByName`
 - :ref:`PR_GetHostByAddr`
 - :ref:`PR_EnumerateHostEnt`
 - :ref:`PR_GetAddrInfoByName`
 - :ref:`PR_EnumerateAddrInfo`
 - :ref:`PR_GetCanonNameFromAddrInfo`
 - :ref:`PR_FreeAddrInfo`

.. _Getting_Protocol_Entries:

Getting protocol entries
~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_GetProtoByName`
 - :ref:`PR_GetProtoByNumber`
