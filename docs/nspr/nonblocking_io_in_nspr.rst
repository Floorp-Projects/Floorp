Nonblocking IO in NSPR
======================


Introduction
------------

Previously, all I/O in the NetScape Portable Runtime (NSPR) was
*blocking* (or *synchronous*). A thread invoking an io function is
blocked until the io operation is finished. The blocking io model
encourages the use of multiple threads as a programming model. A thread
is typically created to attend to one of the simultaneous I/O operations
that may potentially block.

In the *nonblocking* io model, a file descriptor may be marked as
nonblocking. An io function on a nonblocking file descriptor either
succeeds immediately or fails immediately with
<tt>PR_WOULD_BLOCK_ERROR</tt>. A single thread is sufficient to attend
to multiple nonblocking file descriptors simultaneously. Typically, this
central thread invokes <tt>PR_Poll()</tt> on a set of nonblocking file
descriptors. (Note: <tt>PR_Poll()</tt> also works with blocking file
descriptors, although it is less useful in the blocking io model.) When
<tt>PR_Poll()</tt> reports that a file descriptor is ready for some io
operation, the central thread invokes that io function on the file
descriptor.

.. _Creating_a_Nonblocking_Socket:

Creating a Nonblocking Socket
-----------------------------

*Only sockets can be made nonblocking*. Regular files always operate in
blocking mode. This is not a serious constraint as one can assume that
disk I/O never blocks. Fundamentally, this constraint is due to the fact
that nonblocking I/O and <tt>select()</tt> are only available to sockets
on some platforms (e.g., Winsock).

In NSPR, a new socket returned by <tt>PR_NewTCPSocket()</tt> or
<tt>PR_NewUDPSocket()</tt> is always created in blocking mode. One can
make the new socket nonblocking by using <tt>PR_SetSockOpt()</tt> as in
the example below (error checking is omitted for clarity):

|
| <tt>PRFileDesc \*sock;</tt>
| **<tt>PRIntn optval = 1;</tt>**

<tt>sock = PR_NewTCPSocket();</tt>

::

   /*
   * Make the socket nonblocking
   */

   PR_SetSockOpt(sock, PR_SockOpt_Nonblocking, &optval, sizeof(optval));

.. _Programming_Constraints:

Programming Constraints
-----------------------

There are some constraints due to the use of NT asynchronous I/O in the
NSPR. In NSPR, blocking sockets on NT are associated with an I/O
completion port. Once associated with an I/O completion port, we can't
disassociate the socket from the I/O completion port. I have seen some
strange problems with using a nonblocking socket associated with an I/O
completion port. So the first constraint is:

   **The blocking/nonblocking io mode of a new socket is committed the
   first time a potentially-blocking io function is invoked on the
   socket. Once the io mode of a socket is committed, it cannot be
   changed.**

The potentially-blocking io functions include <tt>PR_Connect()</tt>,
<tt>PR_Accept()</tt>, <tt>PR_AcceptRead()</tt>, <tt>PR_Read()</tt>,
<tt>PR_Write()</tt>, <tt>PR_Writev()</tt>, <tt>PR_Recv()</tt>,
<tt>PR_Send()</tt>, <tt>PR_RecvFrom()</tt>, <tt>PR_SendTo()</tt>, and
<tt>PR_TransmitFile(),</tt> and do not include <tt>PR_Bind()</tt> and
<tt>PR_Listen()</tt>.

In blocking mode, any of these potentially-blocking functions requires
the use of the NT I/O completion port. So at that point we must
determine whether to associate the socket with the I/O completion or
not, and that decision cannot be changed later.

There is a second constraint, due to the use of NT asynchronous I/O and
the recycling of used sockets:

   **The new socket returned by <tt>PR_Accept()</tt> or
   <tt>PR_AcceptRead()</tt> inherits the blocking/nonblocking io mode of
   the listening socket and this cannot be changed.**

The socket returned by <tt>PR_Accept()</tt> or <tt>PR_AcceptRead()</tt>
on a blocking, listening socket may be a recycled socket previously used
in a <tt>PR_TransmitFile()</tt> call. Since <tt>PR_TransmitFile()</tt>
only operates in blocking mode, this recycled socket can only be reused
in blocking mode, hence the above constraint.

Because these constraints only apply to NT, it is advised that you test
your cross-platform code that uses nonblocking io on NT early in the
development cycle. These constraints are enforced in the debug NSPR
library by assertions.

.. _Differences_from_Blocking_IO:

Differences from Blocking IO
----------------------------

-  In nonblocking mode, the timeout argument for the io functions is
   ignored.
-  <tt>PR_AcceptRead()</tt> and <tt>PR_TransmitFile()</tt> only work on
   blocking sockets. They do not make sense in nonblocking mode.
-  <tt>PR_Write()</tt>, <tt>PR_Send()</tt>, <tt>PR_Writev()</tt> in
   blocking mode block until the entire buffer is sent. In nonblocking
   mode, they cannot block, so they may return with just sending part of
   the buffer.

.. _PR_Poll()_or_PR_Select():

PR_Poll() or PR_Select()?
-------------------------

<tt>PR_Select()</tt> is deprecated, now declared in
<tt>private/pprio.h</tt>. Use <tt>PR_Poll()</tt> instead.

The current implementation of <tt>PR_Select()</tt> simply calls
<tt>PR_Poll()</tt>, so it is sure to have worse performance. Also,
native file descriptors (socket handles) cannot be added to
<tt>PR_fd_set</tt>, i.e., the functions <tt>PR_FD_NSET</tt>,
<tt>PR_FD_NCLR</tt>, <tt>PR_FD_NISSET</tt> do not work.

PR_Available()
--------------

When <tt>PR_Available()</tt> returns 0, it may mean one of two things:

-  There is no data available for reading on that socket. I.e.,
   <tt>PR_Recv()</tt> would block (a blocking socket) or fail with
   <tt>PR_WOULD_BLOCK_ERROR</tt> (a nonblocking socket).
-  The TCP connection on that socket has been closed (end of stream).

These two cases can be distinguished by <tt>PR_Poll()</tt>. If
<tt>PR_Poll()</tt> reports that the socket is readable (i.e.,
<tt>PR_POLL_READ</tt> is set in <tt>out_flags</tt>), and
<tt>PR_Available()</tt> returns 0, this means that the socket connection
is closed.

.. _Current_Status:

Current Status
--------------

Implemented across all supported platforms.
