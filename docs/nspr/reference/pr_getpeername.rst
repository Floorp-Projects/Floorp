PR_GetPeerName
==============

Gets the network address of the connected peer.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_GetPeerName(
     PRFileDesc *fd,
     PRNetAddr *addr);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing a socket.
``addr``
   On return, the address of the peer connected to the socket.


Returns
~~~~~~~

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The reason for the failure can be
   obtained by calling :ref:`PR_GetError`.
