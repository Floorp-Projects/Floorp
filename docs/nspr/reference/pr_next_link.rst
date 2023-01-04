PR_NEXT_LINK
============

Returns the next element in a list.


Syntax
------

.. code::

   #include <prclist.h>

   PRCList *PR_NEXT_LINK (PRCList *elemp);


Parameter
~~~~~~~~~

``elemp``
   A pointer to the element.


Returns
~~~~~~~

A pointer to a list element.


Description
-----------

PR_NEXT_LINK returns a pointer to the element following the specified
element. It can be used to traverse a list. The following element is not
removed from the list.
