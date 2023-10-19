PRFilePrivate
=============


Layer-dependent implementation data.


Syntax
------

.. code::

   #include <prio.h>

   typedef struct PRFilePrivate PRFilePrivate;


Description
-----------

A layer implementer should collect all the private data of the layer in
the :ref:`PRFilePrivate` structure. Each layer has its own definition of
:ref:`PRFilePrivate`, which is hidden from other layers as well as from the
users of the layer.
