PRIOMethods
===========

The table of I/O methods used in a file descriptor.


Syntax
------

.. code::

   #include <prio.h>

   struct PRIOMethods {
     PRDescType file_type;
     PRCloseFN close;
     PRReadFN read;
     PRWriteFN write;
     PRAvailableFN available;
     PRAvailable64FN available64;
     PRFsyncFN fsync;
     PRSeekFN seek;
     PRSeek64FN seek64;
     PRFileInfoFN fileInfo;
     PRFileInfo64FN fileInfo64;
     PRWritevFN writev;
     PRConnectFN connect;
     PRAcceptFN accept;
     PRBindFN bind;
     PRListenFN listen;
     PRShutdownFN shutdown;
     PRRecvFN recv;
     PRSendFN send;
     PRRecvfromFN recvfrom;
     PRSendtoFN sendto;
     PRPollFN poll;
     PRAcceptreadFN acceptread;
     PRTransmitfileFN transmitfile;
     PRGetsocknameFN getsockname;
     PRGetpeernameFN getpeername;
     PRGetsockoptFN getsockopt;
     PRSetsockoptFN setsockopt;
   };

   typedef struct PRIOMethods PRIOMethods;


Parameters
~~~~~~~~~~

``file_type``
   Type of file represented (tos).
``close``
   Close file and destroy descriptor.
``read``
   Read up to the specified number of bytes into buffer.
``write``
   Write specified number of bytes from buffer.
``available``
   Determine number of bytes available for reading.
``available64``
   Same as previous field, except 64-bit.
``fsync``
   Flush all in-memory buffers of file to permanent store.
``seek``
   Position the file pointer to the desired place.
``seek64``
   Same as previous field, except 64-bit.
``fileInfo``
   Get information about an open file.
``fileInfo64``
   Same as previous field, except 64-bit.
``writev``
   Write from a vector of buffers.
``connect``
   Connect to the specified network address.
``accept``
   Accept a connection from a network peer.
``bind``
   Associate a network address with the file descriptor.
``listen``
   Prepare to listen for network connections.
``shutdown``
   Shut down a network connection.
``recv``
   Receive up to the specified number of bytes.
``send``
   Send all the bytes specified.
``recvfrom``
   Receive up to the specified number of bytes and report network
   source.
``sendto``
   Send bytes to specified network address.
``poll``
   Test the file descriptor to see if it is ready for I/O.
``acceptread``
   Accept and read from a new network file descriptor.
``transmitfile``
   Transmit an entire file to the specified socket.
``getsockname``
   Get network address associated with a file descriptor.
``getpeername``
   Get peer's network address.
``getsockopt``
   Get current setting of specified socket option.
``setsockopt``
   Set value of specified socket option.


Description
-----------

You don't need to know the type declaration for each function listed in
the method table unless you are implementing a layer. For information
about each function, see the corresponding function description in this
document. For example, the ``write`` method in :ref:`PRIOMethods`
implements the :ref:`PR_Write` function. For type definition details, see
``prio.h``.

The I/O methods table provides procedural access to the functions of the
file descriptor. It is the responsibility of a layer implementer to
provide suitable functions at every entry point (that is, for every
function in the I/O methods table). If a layer provides no
functionality, it should call the next lower (higher) function of the
same name (for example, the "close" method would return
``fd->lower->method->close(fd->lower)``).

Not all functions in the methods table are implemented for all types of
files. For example, the seek method is implemented for normal files but
not for sockets. In cases where this partial implementation occurs, the
function returns an error indication with an error code of
``PR_INVALID_METHOD_ERROR``.
