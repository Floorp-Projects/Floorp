PR_GetUniqueIdentity
====================

Asks the runtime to allocate a unique identity for a layer identified by
the layer's name.


Syntax
------

.. code::

   #include <prio.h>

   PRDescIdentity PR_GetUniqueIdentity(const char *layer_name);


Parameter
~~~~~~~~~

The function has the following parameter:

``layer_name``
   The string associated with the creation of a layer's identity.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, the :ref:`PRDescIdentity` for the layer associated with
   the string specified in the layer named ``layer_name``.
-  If the function cannot allocate enough dynamic memory, it fails and
   returns the value ``PR_INVALID_IO_LAYER`` with the error code
   ``PR_OUT_OF_MEMORY_ERROR``.


Description
-----------

A string may be associated with a layer when the layer is created.
:ref:`PR_GetUniqueIdentity` allocates a unique layer identity and
associates it with the string. The string can be subsequently passed to
:ref:`PR_CreateIOLayerStub` to create a new file descriptor of that layer.

Call :ref:`PR_GetUniqueIdentity` only once for any particular layer name.
If you're creating a custom I/O layer, cache the result, and then use
that cached result every time you call :ref:`PR_CreateIOLayerStub`.
