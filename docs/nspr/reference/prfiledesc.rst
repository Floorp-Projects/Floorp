PRFileDesc
==========

A file descriptor used to represent any open file, such as a normal
file, an end point of a pipe, or a socket (end point of network
communication).


Syntax
------

.. code::

   #include <prio.h>

   struct PRFileDesc {
     PRIOMethods *methods;
     PRFilePrivate *secret;
     PRFileDesc *lower, *higher;
     void (*dtor)(PRFileDesc *fd);
     PRDescIdentity identity;
   };

   typedef struct PRFileDesc PRFileDesc;


Parameters
~~~~~~~~~~

``methods``
   The I/O methods table. See :ref:`PRIOMethods`.
``secret``
   Layer-dependent implementation data. See :ref:`PRFilePrivate`.
``lower``
   Pointer to lower layer.
``higher``
   Pointer to higher layer.
``dtor``
   A destructor function for the layer.
``identity``
   Identity of this particular layer. See :ref:`PRDescIdentity`.


Description
-----------

The fields of this structure are significant only if you are
implementing a layer on top of NSPR, such as SSL. Otherwise, you use
functions such as :ref:`PR_Open` and :ref:`PR_NewTCPSocket` to obtain a file
descriptor, which you should treat as an opaque structure.

For more details about the use of :ref:`PRFileDesc` and related structures,
see `File Descriptor Types <I_O_Types#File_Descriptor_Types>`__.
