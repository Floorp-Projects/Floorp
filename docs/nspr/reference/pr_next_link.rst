PR_NEXT_LINK
============

Returns the next element in a list.

.. _Syntax:

Syntax
------

.. code:: eval

   #include <prclist.h>

   PRCList *PR_NEXT_LINK (PRCList *elemp);

.. _Parameter:

Parameter
~~~~~~~~~

``elemp``
   A pointer to the element.

.. _Returns:

Returns
~~~~~~~

A pointer to a list element.

.. _Description:

Description
-----------

PR_NEXT_LINK returns a pointer to the element following the specified
element. It can be used to traverse a list. The following element is not
removed from the list.
