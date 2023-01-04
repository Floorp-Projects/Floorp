PR_Accept
=========

Accepts a connection on a specified socket.


Syntax
------

.. code::

   #include <prio.h>

   PRFileDesc* PR_Accept(
     PRFileDesc *fd,
     PRNetAddr *addr,
     PRIntervalTime timeout);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing the rendezvous
   socket on which the caller is willing to accept new connections.
``addr``
   A pointer to a structure of type :ref:`PRNetAddr`. On output, this
   structure contains the address of the connecting entity.
``timeout``
   A value of type :ref:`PRIntervalTime` specifying the time limit for
   completion of the accept operation.


Returns
~~~~~~~

The function returns one of the following values:

-  Upon successful acceptance of a connection, a pointer to a new
   :ref:`PRFileDesc` structure representing the newly accepted connection.
-  If unsuccessful, ``NULL``. Further information can be obtained by
   calling :ref:`PR_GetError`.


Description
-----------

The socket ``fd`` is a rendezvous socket that has been bound to an
address with :ref:`PR_Bind` and is listening for connections after a call
to :ref:`PR_Listen`. :ref:`PR_Accept` accepts the first connection from the
queue of pending connections and creates a new socket for the newly
accepted connection. The rendezvous socket can still be used to accept
more connections.

If the ``addr`` parameter is not ``NULL``, :ref:`PR_Accept` stores the
address of the connecting entity in the :ref:`PRNetAddr` object pointed to
by ``addr``.

:ref:`PR_Accept` blocks the calling thread until either a new connection is
successfully accepted or an error occurs. If the timeout parameter is
not ``PR_INTERVAL_NO_TIMEOUT`` and no pending connection can be accepted
before the time limit, :ref:`PR_Accept` returns ``NULL`` with the error
code ``PR_IO_TIMEOUT_ERROR``.
