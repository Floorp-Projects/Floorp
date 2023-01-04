PR_Shutdown
===========

Shuts down part of a full-duplex connection on a specified socket.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_Shutdown(
     PRFileDesc *fd,
     PRShutdownHow how);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing a connected socket.
``how``
   The kind of disallowed operations on the socket. Possible values
   include the following:

    - :ref:`PR_SHUTDOWN_RCV`. Further receives will be disallowed.
    - :ref:`PR_SHUTDOWN_SEND`. Further sends will be disallowed.
    - :ref:`PR_SHUTDOWN_BOTH`. Further sends and receives will be
      disallowed.


Returns
~~~~~~~

The function returns one of the following values:

-  Upon successful completion of shutdown request, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. Further information can be obtained
   by calling :ref:`PR_GetError`.


Description
-----------

The ``PRShutdownHow`` enumeration is defined as follows:

.. code::

   typedef enum PRShutdownHow{
     PR_SHUTDOWN_RCV = 0,
     PR_SHUTDOWN_SEND = 1,
     PR_SHUTDOWN_BOTH = 2
   } PRShutdownHow;
