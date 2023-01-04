PR_NETDB_BUF_SIZE
=================

Recommended size to use when specifying a scratch buffer for
:ref:`PR_GetHostByName`, :ref:`PR_GetHostByAddr`, :ref:`PR_GetProtoByName`, or
:ref:`PR_GetProtoByNumber`.


Syntax
------

.. code::

   #include <prnetdb.h>

   #if defined(AIX) || defined(OSF1)
     #define PR_NETDB_BUF_SIZE sizeof(struct protoent_data)
   #else
     #define PR_NETDB_BUF_SIZE 1024
   #endif
