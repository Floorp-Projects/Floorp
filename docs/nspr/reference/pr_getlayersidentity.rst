PR_GetLayersIdentity
====================

Gets the unique identity for the layer of the specified file descriptor.


Syntax
------

.. code::

   #include <prio.h>

   PRDescIdentity PR_GetLayersIdentity(PRFileDesc* fd);


Parameter
~~~~~~~~~

The function has the following parameter:

``fd``
   A pointer to a file descriptor.


Returns
~~~~~~~

If successful, the function returns the :ref:`PRDescIdentity` for the layer
of the specified file descriptor.
