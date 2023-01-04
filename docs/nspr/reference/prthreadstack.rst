PRThreadStack
=============

.. container:: blockIndicator obsolete obsoleteHeader

   | **Obsolete**
   | This feature is obsolete. Although it may still work in some
     browsers, its use is discouraged since it could be removed at any
     time. Try to avoid using it.

The opaque :ref:`PRThreadStack` structure is only used in the third
argument "``PRThreadStack *stack``" to the :ref:`PR_AttachThread` function.
The '``stack``' argument is now obsolete and ignored by
:ref:`PR_AttachThread`. You should pass ``NULL`` as the 'stack' argument to
:ref:`PR_AttachThread`.

.. _Definition:

Syntax
------

.. code::

   #include <prthread.h>

   typedef struct PRThreadStack PRThreadStack;

.. _Definition_2:


-
