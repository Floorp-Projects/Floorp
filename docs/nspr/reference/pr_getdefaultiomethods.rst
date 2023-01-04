PR_GetDefaultIOMethods
======================

Gets the default I/O methods table.


Syntax
------

.. code::

   #include <prio.h>

   const PRIOMethods* PR_GetDefaultIOMethods(void);


Returns
~~~~~~~

If successful, the function returns a pointer to a :ref:`PRIOMethods`
structure.


Description
-----------

After using :ref:`PR_GetDefaultIOMethods` to identify the default I/O
methods table, you can select elements from that table with which to
build your own layer's methods table. You may not modify the default I/O
methods table directly. You can pass your own layer's methods table to
:ref:`PR_CreateIOLayerStub` to create your new layer.
