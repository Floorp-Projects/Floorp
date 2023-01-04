PR_PopIOLayer
=============

Removes a layer from the stack.


Syntax
------

.. code::

   #include <prio.h>

   PRFileDesc *PR_PopIOLayer(
     PRFileDesc *stack,
     PRDescIdentity id);


Parameters
~~~~~~~~~~

The function has the following parameters:

``stack``
   A pointer to a :ref:`PRFileDesc` object representing the stack from
   which the specified layer is to be removed.
``id``
   Identity of the layer to be removed from the stack.


Returns
~~~~~~~

The function returns one of the following values:

-  If the layer is successfully removed from the stack, a pointer to the
   removed layer.
-  If the layer is not found in the stack or cannot be popped (for
   example, the bottommost layer), the function returns ``NULL`` with
   the error code ``PR_INVALID_ARGUMENT_ERROR``.


Description
-----------

:ref:`PR_PopIOLayer` pops the specified layer from the stack. If the object
to be removed is found, :ref:`PR_PopIOLayer` returns a pointer to the
removed object The object then becomes the responsibility of the caller.

Even if the identity indicates the top layer of the stack, the reference
returned is not the file descriptor for the stack and that file
descriptor remains valid. In other words, ``stack`` continues to point
to the top of the stack after the function returns.
