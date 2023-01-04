PR_PushIOLayer
==============

Adds a layer onto the stack.


Syntax
------

.. code::

   #include <prio.h>

   PRStatus PR_PushIOLayer(
     PRFileDesc *stack,
     PRDescIdentity id,
     PRFileDesc *layer);


Parameters
~~~~~~~~~~

The function has the following parameters:

``stack``
   A pointer to a :ref:`PRFileDesc` object representing the stack.
``id``
   A :ref:`PRDescIdentity` object for the layer on the stack above which
   the new layer is to be added.
``layer``
   A pointer to a :ref:`PRFileDesc` object representing the new layer to be
   added to the stack.


Returns
~~~~~~~

The function returns one of the following values:

-  If the layer is successfully pushed onto the stack, ``PR_SUCCESS``.
-  If the layer is not successfully pushed onto the stack,
   ``PR_FAILURE``. Use :ref:`PR_GetError` to get additional information
   regarding the reason for the failure.


Description
-----------

A file descriptor for a layer (possibly allocated using
:ref:`PR_CreateIOLayerStub`) may be pushed onto an existing stack of file
descriptors at any time. The new layer is inserted into the stack just
above the layer with the identity specified by ``id``.

Even if the ``id`` parameter indicates the topmost layer of the stack,
the value of the file descriptor describing the original stack will not
change. In other words, ``stack`` continues to point to the top of the
stack after the function returns.

Caution
-------

Keeping the pointer to the stack even as layers are pushed onto the top
of the stack is accomplished by swapping the contents of the file
descriptor being pushed and the stack's current top layer file
descriptor.

The intent is that the pointer to the stack remain the stack's identity
even if someone (perhaps covertly) has pushed other layers. Some subtle
ramifications:

-  The ownership of the storage pointed to by the caller's layer
   argument is relinquished to the runtime. Accessing the object via the
   pointer is not permitted while the runtime has ownership. The correct
   mechanism to access the object is to get a pointer to it by calling
   :ref:`PR_GetIdentitiesLayer`.

-  The contents of the caller's object are swapped into another
   container, including the reference to the object's destructor. If the
   original container was allocated using a different mechanism than
   used by the runtime, the default calling of the layer's destructor by
   the runtime will fail :ref:`PR_CreateIOLayerStub` is provided to
   allocate layer objects and template implementations). The destructor
   will be called on all layers when the stack is closed (see
   :ref:`PR_Close`). If the containers are allocated by some method other
   than :ref:`PR_CreateIOLayerStub`, it may be required that the stack have
   the layers popped off (in reverse order that they were pushed) before
   calling :ref:`PR_Close`.
