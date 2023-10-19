PRDescIdentity
==============

The identity of a file descriptor's layer.


Syntax
------

.. code::

   #include <prio.h>

   typedef PRUintn PRDescIdentity;


Description
-----------

File descriptors may be layered. Each layer has it own identity.
Identities are allocated by the runtime and are to be associated (by the
layer implementer) with all file descriptors of that layer. It is then
possible to scan the chain of layers and find a layer that one
recognizes, then predict that it will implement a desired protocol.

There are three well-known identities:

 - :ref:`PR_INVALID_IO_LAYER`, an invalid layer identity, for error return
 - :ref:`PR_TOP_IO_LAYER`, the identity of the top of the stack
 - :ref:`PR_NSPR_IO_LAYER`, the identity used by NSPR proper

Layers are created by :ref:`PR_GetUniqueIdentity`. A string may be
associated with a layer when the layer is created. The string is copied
by the runtime, and :ref:`PR_GetNameForIdentity` returns a reference to
that copy. There is no way to delete a layer's identity after the layer
is created.
