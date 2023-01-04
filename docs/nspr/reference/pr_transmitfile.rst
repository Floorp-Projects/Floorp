PR_TransmitFile
===============

Sends a complete file across a connected socket.


Syntax
------

.. code::

   #include <prio.h>

   PRInt32 PR_TransmitFile(
     PRFileDesc *networkSocket,
     PRFileDesc *sourceFile,
     const void *headers,
     PRInt32 hlen,
     PRTransmitFileFlags flags,
     PRIntervalTime timeout);


Parameters
~~~~~~~~~~

The function has the following parameters:

``networkSocket``
   A pointer to a :ref:`PRFileDesc` object representing the connected
   socket to send data over.
``sourceFile``
   A pointer to a :ref:`PRFileDesc` object representing the file to send.
``headers``
   A pointer to the buffer holding the headers to be sent before sending
   data.
``hlen``
   Length of the ``headers`` buffer in bytes.
``flags``
   One of the following flags:

 - :ref:`PR_TRANSMITFILE_KEEP_OPEN` indicates that the socket will be kept
   open after the data is sent.
 - :ref:`PR_TRANSMITFILE_CLOSE_SOCKET` indicates that the connection should
   be closed immediately after successful transfer of the file.

``timeout``
   Time limit for completion of the transmit operation.


Returns
~~~~~~~

-  A positive number indicates the number of bytes successfully written,
   including both the headers and the file.
-  The value -1 indicates a failure. If an error occurs while sending
   the file, the ``PR_TRANSMITFILE_CLOSE_SOCKET`` flag is ignored. The
   reason for the failure can be obtained by calling :ref:`PR_GetError`.


Description
-----------

The :ref:`PR_TransmitFile` function sends a complete file (``sourceFile``)
across a connected socket (``networkSocket``). If ``headers`` is
non-``NULL``, :ref:`PR_TransmitFile` sends the headers across the socket
before sending the file.

The enumeration ``PRTransmitFileFlags``, used in the ``flags``
parameter, is defined as follows:

.. code::

   typedef enum PRTransmitFileFlags {
     PR_TRANSMITFILE_KEEP_OPEN = 0,
     PR_TRANSMITFILE_CLOSE_SOCKET = 1
   } PRTransmitFileFlags;
