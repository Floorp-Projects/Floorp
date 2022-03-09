Non-blocking layered I/O
========================

*[last edited by AOF 24 March 1998 14:15]*
I've recently been working on a long standing issue regarding NSPR's I/O
model. For a long time I've believed that the non-blocking I/O prevalent
in classic operating systems (e.g., UNIX) was the major determent for
having an reasonable layered protocols. Now that I have some first hand
experience, albeit just a silly little test program, I am more convinced
that ever of this truth.

This memo is some of what I think must be done in NSPR's I/O subsystem
to make layered, non-blocking protocols workable. It is just a proposal.
There is an API change.

Layered I/O
-----------

NSPR 2.0 defines a structure by which one may define I/O layers. Each
layer looks basically like any other in that it still uses a
:ref:`PRFileDesc` as a object identifier, complete with the
**``IOMethods``** table of functions. However, each layer may override
default behavior of a particular operation to implement other services.
For instance, the experiment at hand is one that implements a little
reliable echo protocol; the client sends *n* bytes, and the same bytes
get echoed back by the server. In the non-layered design of this it is
straight forward.
The goal of the experiment was to put a layer between the client and
the network, and not have the client know about it. This additional
layer is one that, before sending the client's data, must ask permission
from the peer layer to send that many bytes. It imposes an additional
send and response inside of each client visible send operation. The
receive operations parallel the sends. Before actually receiving real
client data, the layer receives a notification that the other would like
to send some bytes. The layer is responsible for granting permission for
that data to be sent, then actually receiving the data itself, which is
delivered to the client.

The synchronous form of the layer's operation is straight forward. A
call to receive (:ref:`PR_Recv`) first receives the request to send,
sends (:ref:`PR_Send`) the grant, then receives the actual data
(:ref:`PR_Recv`). All the client of the layer sees is the data coming
in. Similar behavior is observed on the sending side.

Non-blocking layered
--------------------

The non-blocking method is not so simple. Any of the I/O operations
potentially result in an indication that no progress can be made. The
intermediate layers cannot act directly on this information, but must
store the state of the I/O operation until it can be resumed. The method
for determining that a I/O operation can make progress is to call
:ref:`PR_Poll` and indicating what type of progress is desired,
either input or output (or some others). Therein lies the problem.
The intermediate layer is performing operations that the client is
unaware. So when the client calls send (:ref:`PR_Send`) and is told
that the operation would block, it is possible that the layer below is
actually doing a receive (:ref:`PR_Recv`). The problem is that the
flag bits passed to :ref:`PR_Poll` are only reflective of the
client's knowledge and desires. This is further complicated by the fact
that :ref:`PR_Poll` is not layered. That is each layer does not have
the opportunity to override the behavior. It operates, not on a single
file descriptor (:ref:`PRFileDesc`), but on an arbitrary collection of
file descriptors.

Into the picture comes another I/O method, **``poll()``**. Keep in mind
that all I/O methods are those that are part of the I/O methods table
structure (:ref:`PRIOMethods`). These functions are layered, and layers
may and sometimes must override their behavior by offering unique
implementations. The **``poll()``** method is used to provide two
modifying aspects to the semantics of :ref:`PR_Poll`: redefining the
polling bits (i.e., what to poll for) and to indicate that a layer is
already able to make progress in the manner suggested by the polling
bits.

The **``poll()``** method is called by :ref:`PR_Poll` as the latter
is building the structure to provide the operating system call. The
stack's top layer will be called first. Each layer's implementation is
responsible for performing appropriate operations and possibly calling
the next lower layer's **``poll()``** method.
What the poll method is returning are the appropriate flags to assign to
the operating system's call. A layer would compute these based on the
values of the argument **``in_flags``** and possibly some state
maintained by the layer for the particular file descriptor.

Additionally, if the layer has buffered data that will allow the
operation defined by **``in_flags``** to make progress, it will set
corresponding bits in **``out_flags``**. For instance, if
**``in_flags``** indicates that the client (or higher layer) wishes to
test for read ready and the layer has input data buffered, it would set
the read bits in the **``out_flags``**. If that is the case, then it
should also suppress the calling of the next lower layer's
**``poll()``** method and return a value equal to that of
**``in_flags``**.
