PR_INSERT_LINK
==============

Inserts an element at the head of the list.


Syntax
------

.. code::

   #include <prclist.h>

   PR_INSERT_LINK (
     PRCList *elemp,
     PRCList *listp);


Parameters
~~~~~~~~~~

``elemp``
   A pointer to the element to be inserted.
``listp``
   A pointer to the list.


Description
-----------

PR_INSERT_LINK inserts the specified element at the head of the
specified list.
