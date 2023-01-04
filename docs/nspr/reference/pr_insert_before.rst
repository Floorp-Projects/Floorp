PR_INSERT_BEFORE
================

Inserts an element before another element in a circular list.


Syntax
------

.. code::

   #include <prclist.h>

   PR_INSERT_BEFORE (
      PRCList *elemp1
      PRCList *elemp2);


Parameters
~~~~~~~~~~

``elemp1``
   A pointer to the element to be inserted.
``elemp2``
   A pointer to the element before which ``elemp1`` is to be inserted.


Description
-----------

PR_INSERT_BEFORE inserts the element specified by ``elemp1`` into the
circular list, before the element specified by ``elemp2``.
