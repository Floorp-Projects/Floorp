PR_GetDescType
==============

Describes what type of file is referenced by a specified file
descriptor.


Syntax
------

.. code::

   #include <prio.h>

   PRDescType PR_GetDescType(PRFileDesc *file);


Parameter
~~~~~~~~~

The function has the following parameter:

``file``
   A pointer to a :ref:`PRFileDesc` object whose descriptor type is to be
   returned.


Returns
~~~~~~~

The function returns a ``PRDescType`` enumeration constant that
describes the type of file.


Description
-----------

The ``PRDescType`` enumeration is defined as follows:

.. code::

   typedef enum PRDescType {
     PR_DESC_FILE       = 1,
     PR_DESC_SOCKET_TCP = 2,
     PR_DESC_SOCKET_UDP = 3,
     PR_DESC_LAYERED    = 4
   } PRDescType;

The enumeration has the following enumerators:

``PR_DESC_FILE``
   The :ref:`PRFileDesc` object represents a normal file.
``PR_DESC_SOCKET_TCP``
   The :ref:`PRFileDesc` object represents a TCP socket.
``PR_DESC_SOCKET_UDP``
   The :ref:`PRFileDesc` object represents a UDP socket.
``PR_DESC_LAYERED``
   The :ref:`PRFileDesc` object is a layered file descriptor.
