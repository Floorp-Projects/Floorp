PRHostEnt
=========

A structure that defines a list of network addresses. This structure is
output from :ref:`PR_GetHostByName` and :ref:`PR_GetHostByAddr` and passed to
:ref:`PR_EnumerateHostEnt`. Clients should avoid directly accessing any of
the structure's fields.


Syntax
------

.. code::

   #include <prnetdb.h>

   typedef struct PRHostEnt {
     char *h_name;
     char **h_aliases;
   #if defined(_WIN32)
     PRInt16 h_addrtype;
     PRInt16 h_length;
   #else
     PRInt32 h_addrtype;
     PRInt32 h_length;
   #endif
     char **h_addr_list;
   } PRHostEnt;


Fields
~~~~~~

The structure has the following fields:

``h_name``
   Pointer to the official name of host.
``h_aliases``
   Pointer to a pointer to list of aliases. The list is terminated with
   a ``NULL`` entry.
``h_addrtype``
   Host address type. For valid NSPR usage, this field must have a value
   indicating either an IPv4 or an IPv6 address.
``h_length``
   Length of internal representation of the address in bytes. All of the
   addresses in the list are of the same type and therefore of the same
   length.
``h_addr_list``
   Pointer to a pointer to a list of addresses from name server (in
   network byte order). The list is terminated with a ``NULL`` entry.


Description
-----------

This structure is used by many of the network address functions. All
addresses are passed in host order and returned in network order
(suitable for use in system calls).

Use the network address functions to manipulate the :ref:`PRHostEnt`
structure. To make the transition to IP version 6 easier, it's best to
treat :ref:`PRHostEnt` as an opaque structure.

Note
----

``WINSOCK.H`` defines ``h_addrtype`` and ``h_length`` as a 16-bit field,
whereas other platforms treat it as a 32-bit field. The ``#ifdef`` in
the structure allows direct assignment of the :ref:`PRHostEnt` structure.
