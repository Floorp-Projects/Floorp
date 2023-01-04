PR_GetHostByName
================

Looks up a host by name.


Syntax
------

.. code::

   #include <prnetdb.h>

   PRStatus PR_GetHostByName(
     const char *hostname,
     char *buf,
     PRIntn bufsize,
     PRHostEnt *hostentry);


Parameters
~~~~~~~~~~

The function has the following parameters:

``hostname``
   The character string defining the host name of interest.
``buf``
   A pointer to a buffer, allocated by the caller, that is filled in
   with host data on output. All of the pointers in the ``hostentry``
   structure point to data saved in this buffer. This buffer is
   referenced by the runtime during a call to :ref:`PR_EnumerateHostEnt`.
``bufsize``
   Number of bytes in the ``buf`` parameter. The buffer must be at least
   :ref:`PR_NETDB_BUF_SIZE` bytes.
``hostentry``
   This structure is allocated by the caller. On output, this structure
   is filled in by the runtime if the function returns ``PR_SUCCESS``.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, ``PR_SUCCESS``.
-  If unsuccessful, ``PR_FAILURE``. You can retrieve the reason for the
   failure by calling :ref:`PR_GetError`.
