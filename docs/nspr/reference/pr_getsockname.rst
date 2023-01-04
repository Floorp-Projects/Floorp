PR_GetSockName
==============

Gets network address for a specified socket.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_GetSockName(
     PRFileDesc *fd,
     PRNetAddr *addr);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing the socket.
``addr``
   On return, the address of the socket.


Returns
~~~~~~~

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The reason for the failure can be
   obtained by calling :ref:`PR_GetError`.
