PR_APPEND_LINK
==============

Appends an element to the end of a list.


Syntax
------

.. code::

   #include <prclist.h>

   PR_APPEND_LINK (
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

PR_APPEND_LINK adds the specified element to the end of the specified
list.
