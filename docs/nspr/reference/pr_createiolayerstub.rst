PR_CreateIOLayerStub
====================

Creates a new layer.


Syntax
------

.. code::

   #include <prio.h>

   PRFileDesc* PR_CreateIOLayerStub(
     PRDescIdentity ident
     PRIOMethods const *methods);


Parameters
~~~~~~~~~~

The function has the following parameters:

``ident``
   The identity to be associated with the new layer.
``methods``
   A pointer to the :ref:`PRIOMethods` structure specifying the functions
   for the new layer.


Returns
~~~~~~~

A new file descriptor for the specified layer.


Description
-----------

A new layer may be allocated by calling :ref:`PR_CreateIOLayerStub`. The
file descriptor returned contains the pointer to the I/O methods table
provided. The runtime neither modifies the table nor tests its
correctness.

The caller should override appropriate contents of the file descriptor
returned before pushing it onto the protocol stack.
