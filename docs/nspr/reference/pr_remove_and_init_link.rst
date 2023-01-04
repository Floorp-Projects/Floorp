PR_REMOVE_AND_INIT_LINK
=======================

Removes an element from a circular list and initializes the linkage.


Syntax
------

.. code::

   #include <prclist.h>

   PR_REMOVE_AND_INIT_LINK (PRCList *elemp);


Parameter
~~~~~~~~~

``elemp``
   A pointer to the element.


Description
-----------

:ref:`PR_REMOVE_AND_INIT_LINK` removes the specified element from its
circular list and initializes the links of the element to point to
itself.
