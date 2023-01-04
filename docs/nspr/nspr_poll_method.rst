NSPR pool method
================

This technical note documents the poll method of PRFileDesc. The poll
method is not to be confused with the PR_Poll function. The poll method
operates on a single NetScape Portable Runtime (NSPR) file descriptor,
whereas PR_Poll operates on a collection of NSPR file descriptors.
PR_Poll uses the poll method behind the scene, but it is also possible
to use the poll method directly.

We consider a stack of *NSPR I/O layers* on top of the *network
transport*. Each I/O layer is represented by a PRFileDesc structure and
the protocol of that layer is implemented by a PRIOMethods table. The
bottom layer is a wrapper for the underlying network transport. The NSPR
library provides a reference implementation of the bottom layer using
the sockets API, but you can provide your own implementation of the
bottom layer using another network transport API. The poll method is one
of the functions in the PRIOMethods table. The prototype of the poll
method is

.. code::

   PRInt16 poll_method(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags);

The purpose of the poll method is to allow a layer to modify that flags
that will ultimately be used in the call to the underlying network
transport's select (or equivalent) function, and to indicate that a
layer is already able to make progress in the manner suggested by the
polling flags. The arguments and return value of the poll method are
described below.

.. _in_flags_input_argument:

in_flags [input argument]
~~~~~~~~~~~~~~~~~~~~~~~~~

The in_flags argument specifies the events at the **top layer** of the
I/O layer stack that the caller is interested in.

-  For PR_Recv, you should pass PR_POLL_READ as the in_flags argument to
   the poll method
-  For PR_Send, you should pass PR_POLL_WRITE as the in_flags argument
   to the poll method

.. _out_flags_output_argument:

out_flags [output argument]
~~~~~~~~~~~~~~~~~~~~~~~~~~~

If an I/O layer is ready to satisfy the I/O request defined by in_flags
without involving the underlying network transport, its poll method sets
the corresponding event in \*out_flags on return.

For example, consider an I/O layer that buffers input data. If the
caller wishes to test for read ready (that is, PR_POLL_READ is set in
in_flags) and the layer has input data buffered, the poll method would
set the PR_POLL_READ event in \*out_flags. It can determine that without
asking the underlying network transport.

The current implementation of PR_Poll (the primary user of the poll
method) requires that the events in \*out_flags reflect the caller's
view. This requirement may be relaxed in a future NSPR release. To
remain compatible with this potential semantic change, NSPR clients
should only use \*out_flags as described in the *How to use the poll
method* section below.

.. _Return_value:

Return value
~~~~~~~~~~~~

If the poll method stores a nonzero value in \*out_flags, the return
value will be the value of in_flags. (Note: this may change in a future
NSPR release if we make the semantic change to \*out_flags mentioned
above. Therefore, NSPR clients should only use the return value as
described in *How to use the poll method* section below.) If the poll
method stores zero in \*out_flags, the return value will be the bottom
layer's desires with respect to the in_flags. Those are the events that
the caller should poll the underlying network transport for. These
events may be different from the events in in_flags (which reflect the
caller's view) for some protocols.

.. _How_to_use_the_poll_method:

How to use the poll method
~~~~~~~~~~~~~~~~~~~~~~~~~~

The poll method should only be used with a NSPR file descriptor in
**non-blocking** mode. Most NSPR clients call PR_Poll and do not call
the poll method directly. However, PR_Poll can only used with a stack
whose bottom layer is NSPR's reference implementation. If you are using
your own implementation of the bottom layer, you must call the poll
method as follows.

Declare two PRInt16 variables to receive the return value and the
out_flags output argument of the poll method.

.. code::

   PRInt16 new_flags, out_flags;

If you are going to call PR_Recv, pass PR_POLL_READ as the in_flags
argument.

.. code::

   new_flags = fd->methods->poll(fd, PR_POLL_READ, &out_flags);

If you are going to call PR_Send, pass PR_POLL_WRITE as the in_flags
argument.

.. code::

   new_flags = fd->methods->poll(fd, PR_POLL_WRITE, &out_flags);

If you are interested in calling both PR_Recv and PR_Send on the same
file descriptor, make two separate calls to the poll method, one with
PR_POLL_READ as in_flags and the other with PR_POLL_WRITE as in_flags,
so that you know what events at the network transport layer PR_POLL_READ
and PR_POLL_WRITE are mapped to, respectively.

On return, if (new_flags & out_flags) is nonzero, you can try PR_Recv or
PR_Send immediately.

Otherwise ((new_flags & out_flags) is 0), you should do the following.

-  If new_flags contains PR_POLL_READ, you should try PR_Recv or PR_Send
   when the underlying network transport is readable
-  If new_flags contains PR_POLL_WRITE, you should try PR_Recv or
   PR_Send when the underlying network transport is writable

**Important** do not use out_flags in any way other than testing if
(new_flags & out_flags) is 0. This is how PR_Poll (the primary user and
hence the de facto specification of the poll method) uses out_flags.
