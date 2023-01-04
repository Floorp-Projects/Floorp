PR_LIST_TAIL
============

Returns the tail of a circular list.


Syntax
------

.. code::

   #include <prclist.h>

   PRCList *PR_LIST_TAIL (PRCList *listp);


Parameter
~~~~~~~~~

``listp``
   A pointer to the linked list.


Returns
~~~~~~~

A pointer to a list element.


Description
-----------

:ref:`PR_LIST_TAIL` returns the tail of the specified circular list.
