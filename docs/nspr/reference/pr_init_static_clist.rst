PR_INIT_STATIC_CLIST
====================

Statically initializes a circular list.


Syntax
------

.. code::

   #include <prclist.h>

   PR_INIT_STATIC_CLIST (PRCList *listp);


Parameter
~~~~~~~~~

``listp``
   A pointer to the anchor of the linked list.


Description
-----------

PR_INIT_STATIC_CLIST statically initializes the specified list to be an
empty list. For example,

::

   PRCList free_object_list = PR_INIT_STATIC_CLIST(&free_object_list);
