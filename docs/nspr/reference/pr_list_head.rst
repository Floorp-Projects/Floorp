PR_LIST_HEAD
============

Returns the head of a circular list.


Syntax
------

.. code::

   #include <prclist.h>

   PRCList *PR_LIST_HEAD (PRCList *listp);


Parameter
~~~~~~~~~

``listp``
   A pointer to the linked list.


Returns
~~~~~~~

A pointer to a list element.


Description
-----------

:ref:`PR_LIST_HEAD` returns the head of the specified circular list.
