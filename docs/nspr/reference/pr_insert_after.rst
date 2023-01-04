PR_INSERT_AFTER
===============

Inserts an element after another element in a circular list.


Syntax
------

.. code::

   #include <prclist.h>

   PR_INSERT_AFTER (
      PRCList *elemp1
      PRCList *elemp2);


Parameters
~~~~~~~~~~

``elemp1``
   A pointer to the element to be inserted.
``elemp2``
   A pointer to the element after which ``elemp1`` is to be inserted.


Description
-----------

PR_INSERT_AFTER inserts the element specified by ``elemp1`` into the
circular list, after the element specified by ``elemp2``.
