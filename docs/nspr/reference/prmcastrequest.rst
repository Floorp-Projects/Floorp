PRMcastRequest
==============

Structure used to specify values for the ``PR_SockOpt_AddMember`` and
``PR_SockOpt_DropMember`` socket options that define a request to join
or leave a multicast group.


Syntax
------

.. code::

   #include <prio.h>

   struct PRMcastRequest {
     PRNetAddr mcaddr;
     PRNetAddr ifaddr;
   };

   typedef struct PRMcastRequest PRMcastRequest;


Fields
~~~~~~

The structure has the following fields:

``mcaddr``
   IP multicast address of group.
``ifaddr``
   Local IP address of interface.


Description
-----------

The ``mcaddr`` and ``ifaddr`` fields are of the type :ref:`PRNetAddr`, but
their ``port`` fields are ignored. Only the IP address (``inet.ip``)
fields are used.
