PRSockOption
============

Enumeration type used in the ``option`` field of :ref:`PRSocketOptionData`
to form the name portion of a name-value pair.


Syntax
------

.. code::

   #include <prio.h>

   typedef enum PRSockOption {
     PR_SockOpt_Nonblocking,
     PR_SockOpt_Linger,
     PR_SockOpt_Reuseaddr,
     PR_SockOpt_Keepalive,
     PR_SockOpt_RecvBufferSize,
     PR_SockOpt_SendBufferSize,
     PR_SockOpt_IpTimeToLive,
     PR_SockOpt_IpTypeOfService,
     PR_SockOpt_AddMember,
     PR_SockOpt_DropMember,
     PR_SockOpt_McastInterface,
     PR_SockOpt_McastTimeToLive,
     PR_SockOpt_McastLoopback,
     PR_SockOpt_NoDelay,
     PR_SockOpt_MaxSegment,
     PR_SockOpt_Last
   } PRSockOption;


Enumerators
~~~~~~~~~~~

The enumeration has the following enumerators:

``PR_SockOpt_Nonblocking``
   Nonblocking I/O.
``PR_SockOpt_Linger``
   Time to linger on close if data is present in the socket send buffer.
``PR_SockOpt_Reuseaddr``
   Allow local address reuse.
``PR_SockOpt_Keepalive``
   Periodically test whether connection is still alive.
``PR_SockOpt_RecvBufferSize``
   Receive buffer size.
``PR_SockOpt_SendBufferSize``
   Send buffer size.
``PR_SockOpt_IpTimeToLive``
   IP time-to-live.
``PR_SockOpt_IpTypeOfService``
   IP type-of-service and precedence.
``PR_SockOpt_AddMember``
   Join an IP multicast group.
``PR_SockOpt_DropMember``
   Leave an IP multicast group.
``PR_SockOpt_McastInterface``
   IP multicast interface address.
``PR_SockOpt_McastTimeToLive``
   IP multicast time-to-live.
``PR_SockOpt_McastLoopback``
   IP multicast loopback.
``PR_SockOpt_NoDelay``
   Disable Nagle algorithm. Don't delay send to coalesce packets.
``PR_SockOpt_MaxSegment``
   Maximum segment size.
``PR_SockOpt_Last``
   Always one greater than the maximum valid socket option numerator.


Description
-----------

The :ref:`PRSockOption` enumeration consists of all the socket options
supported by NSPR. The ``option`` field of :ref:`PRSocketOptionData` should
be set to an enumerator of type :ref:`PRSockOption`.
