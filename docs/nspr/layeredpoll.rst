PR_Poll() and the layered I/O
=============================

*[last edited by AOF 8 August 1998]*
This memo discusses some of the nuances of using PR_Poll() in
conjunction with *layered I/O*. This is a relatively new feature in NSPR
2.0, not that it hasn't been in the source tree for a while, but in that
it has had no clients.

Implementation
--------------

NSPR provides a public API function, PR_Poll() that is modeled after
UNIX' ``poll()`` system call.

The implementation of :ref:`PR_Poll` is somewhat complicated. Not only
does it map the :ref:`PRPollDesc` array into structures needed by the
underlying OS, it also must deal with layered I/O. This is done despite
the fact that :ref:`PR_Poll` itself is *not* layered. For every element
of the :ref:`PRPollDesc` array that has a non-NULL :ref:`PRFileDesc` and whose
``in_flags`` are not zero, it calls the file descriptor's
``poll() method``.
The ``poll()`` method is one of the vector contained in the
:ref:`PRIOMethods` table. In the case of layered I/O, the elements (the
methods) of the methods table may be overridden by the implementer of
that layer. The layers are then *stacked.* I/O using that *stack* will
call through the method at the top layer, and each layer may make
altering decisions regarding how the I/O operation should proceed.

The purpose of the ``poll()`` method is to allow a layer to modify the
flags that will ultimately be used in the call to the underlying OS'
``poll()`` (or equivalent) function. Such modification might be useful
if one was implementing an augmented stream protocol (*e.g.,* **SSL**).
SSL stands for **Secure Socket Layer**, hence the obvious applicability
as an example. But it is way to complicated to describe in this memo, so
this memo will use a much simpler layered protocol.
The example protocol is one that, in order to send *n* bytes, it must
first ask the connection's peer if the peer is willing to receive that
many bytes. The form of the request is 4 bytes (binary) stating the
number of bytes the sender wishes to transmit. The peer will send back
the number of bytes it is willing to receive (in the test code there are
no error conditions, so don't even ask).

The implication of the protocol is obvious. In order to do a
:ref:`PR_Send` operation, the layer must first do a *different* send and
then *receive* a response. Doing this and keeping the *stack's* client
unaware is the goal. **It is not a goal of NSPR 2.0 to hide the nuances
of synchronous verses non-blocking I/O**.

The layered methods
-------------------

Each layer must implement a suitable function for *every* element of the
methods table. One can get a copy of default methods by calling
:ref:`PR_GetDefaultIOMethods` These methods simply pass all calls
through the layer on to the next lower layer of the stack.

A layer implementer might copy the elements of the ``PRIOMethods``
acquired from this function into a methods table of its own, then
override just those methods of interest. *Usually* (with only a single
exception) a layered method will perform its design duties and then call
the next lower layer's equivalent function.

Layered ``poll()``
------------------

One of the more interesting methods is the ``poll()``. It is called by
the runtime whenever the client calls :ref:`PR_Poll`. It may be called at
the *top* layer for *every* file descriptor in the poll descriptor. It
may be called zero or more times. The purpose of the ``poll()`` method
is to provide the layer an opportunity to adjust the polling bits as
needed. For instance, if a client (*i.e.*, top layer) is calling
:ref:`PR_Poll` for a particular file descriptor with a *read* poll
request, a lower layer might decide that it must perform a *write*
first.
In that case, the layer's ``poll()`` method would be called with
**``in_flags``** including a ``PR_POLL_READ`` flag. However, the
``poll()`` method would call the next lower layer's ``poll()`` method
with a ``PR_POLL_WRITE`` bit set. This process of re-assigning the poll
flags can happen as many times as there are layers in the stack. It is
the final value, the one returned to the caller of the top layer's
``poll()`` method (:ref:`PR_Poll`) that will be used by the runtime when
calling the OS' ``poll()`` (or equivalent) system call.

It is expected that the modification of the polling bits propagate from
the top of the stack down, allowing the layer closest to the bottom of
the stack to provide the final setting. The implication is that there
should be no modifications of the **``in_flags``** during the *return*
phase of the layered function.

For example:

It is not advised to modify the ``final_in_flags`` between the call to
the lower layer's ``poll()`` method and the ``return`` statement.
The third argument of the ``poll()`` method is a pointer to a 16-bit
word. If the layer sets a value in memory through that pointer *and*
returns with a value that has *corresponding* bits, the runtime assumes
that the file descriptor is ready immediately.

There are two important deviations from the normal. First, this is the
one (known) exception to having a layered routine call the stack's next
lower layer method. If bits are set in the ``out_flags`` the method
should return *directly*. Second, the runtime will observe that the
layer claims this file descriptor is ready and suppress the call to the
OS' ``poll()`` system call.

At this time the only known use for this feature is to allow a layer to
indicate it has buffered *input*. Note that it is not appropriate for
buffered *output* since in order to write/send output the runtime must
still confirm with the OS that such an operation is permitted.

Since the ``poll()`` method may be called zero or more times it must
therefore be *idempotent* or at least *functional*. It will need to look
at the layer's state, but must not make modifications to that state that
would cause subsequent calls within the same :ref:`PR_Poll` call to
return a different answer. Since the ``poll()`` method may not be called
at all, so there is not guarantee that any modifications that would have
been performed by the routine will every happen.
