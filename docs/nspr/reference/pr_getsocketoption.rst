PR_GetSocketOption
==================

Retrieves the socket options set for a specified socket.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_GetSocketOption(
     PRFileDesc *fd,
     PRSocketOptionData *data);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing the socket whose
   options are to be retrieved.
``data``
   A pointer to a structure of type :ref:`PRSocketOptionData`. On input,
   the ``option`` field of this structure must be set to indicate which
   socket option to retrieve for the socket represented by the ``fd``
   parameter. On output, this structure contains the requested socket
   option data.


Returns
~~~~~~~

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The reason for the failure can be
   obtained by calling :ref:`PR_GetError`.
