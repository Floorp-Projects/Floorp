PR_CLIST_IS_EMPTY
=================

Checks for an empty circular list.


Syntax
------

.. code::

   #include <prclist.h>

   PRIntn PR_CLIST_IS_EMPTY (PRCList *listp);


Parameter
~~~~~~~~~

``listp``
   A pointer to the linked list.


Description
-----------

PR_CLIST_IS_EMPTY returns a non-zero value if the specified list is an
empty list, otherwise returns zero.
