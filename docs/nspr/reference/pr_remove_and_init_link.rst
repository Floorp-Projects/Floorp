PR_REMOVE_AND_INIT_LINK
=======================

Removes an element from a circular list and initializes the linkage.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prclist.h>

   PR_REMOVE_AND_INIT_LINK (PRCList *elemp);

.. _Parameter:

Parameter
~~~~~~~~~

``elemp``
   A pointer to the element.

.. _Description:

Description
-----------

:ref:`PR_REMOVE_AND_INIT_LINK` removes the specified element from its
circular list and initializes the links of the element to point to
itself.
