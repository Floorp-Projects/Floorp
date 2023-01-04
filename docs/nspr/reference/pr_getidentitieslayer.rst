PR_GetIdentitiesLayer
=====================

Finds the layer with the specified identity in the specified stack of
layers.


Syntax
------

.. code::

   #include <prio.h>

   PRFileDesc* PR_GetIdentitiesLayer(
     PRFileDesc* stack,
     PRDescIdentity id);


Parameters
~~~~~~~~~~

The function has the following parameters:

``stack``
   A pointer to a :ref:`PRFileDesc` object that is a layer in a stack of
   layers.
``id``
   The identity of the specified layer.


Returns
~~~~~~~

The function returns one of the following values:

-  If successful, a pointer to a file descriptor of the layer with the
   specified identity in the given stack of layers.
-  If not successful, ``NULL``.


Description
-----------

The stack of layers to be searched is specified by the fd parameter,
which is a layer in the stack. Both the layers underneath fd and the
layers above fd are searched to find the layer with the specified
identity.
