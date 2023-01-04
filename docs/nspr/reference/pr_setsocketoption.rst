PR_SetSocketOption
==================

Retrieves the socket options set for a specified socket.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_SetSocketOption(
     PRFileDesc *fd,
     PRSocketOptionData *data);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing the socket whose
   options are to be set.
``data``
   A pointer to a structure of type :ref:`PRSocketOptionData` specifying
   the options to set.


Returns
~~~~~~~

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. The reason for the failure can be
   obtained by calling :ref:`PR_GetError`.


Description
-----------

On input, the caller must set both the ``option`` and ``value`` fields
of the :ref:`PRSocketOptionData` object pointed to by the ``data``
parameter.
