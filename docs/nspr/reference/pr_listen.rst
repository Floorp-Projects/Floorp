PR_Listen
=========

Listens for connections on a specified socket.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_Listen(
     PRFileDesc *fd,
     PRIntn backlog);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing a socket that will
   be used to listen for new connections.
``backlog``
   The maximum length of the queue of pending connections.


Returns
~~~~~~~

The function returns one of the following values:

-  Upon successful completion of listen request, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. Further information can be obtained
   by calling :ref:`PR_GetError`.


Description
-----------

:ref:`PR_Listen` turns the specified socket into a rendezvous socket. It
creates a queue for pending connections and starts to listen for
connection requests on the socket. The maximum size of the queue for
pending connections is specified by the ``backlog`` parameter. Pending
connections may be accepted by calling :ref:`PR_Accept`.
