PR_Bind
=======

Binds an address to a specified socket.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_Bind(
     PRFileDesc *fd,
     const PRNetAddr *addr);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing a socket.
``addr``
   A pointer to a :ref:`PRNetAddr` object representing the address to which
   the socket will be bound.


Returns
~~~~~~~

The function returns one of the following values:

-  Upon successful binding of an address to a socket, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. Further information can be obtained
   by calling :ref:`PR_GetError`.


Description
-----------

When a new socket is created, it has no address bound to it. :ref:`PR_Bind`
assigns the specified address (also known as name) to the socket. If you
do not care about the exact IP address assigned to the socket, set the
``inet.ip`` field of :ref:`PRNetAddr` to :ref:`PR_htonl`\ (``PR_INADDR_ANY``).
If you do not care about the TCP/UDP port assigned to the socket, set
the ``inet.port`` field of :ref:`PRNetAddr` to 0.

Note that if :ref:`PR_Connect` is invoked on a socket that is not bound, it
implicitly binds an arbitrary address the socket.

Call :ref:`PR_GetSockName` to obtain the address (name) bound to a socket.
