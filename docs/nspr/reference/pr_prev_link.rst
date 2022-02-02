PR_PREV_LINK
============

Returns the preceding element in a list.

.. _Syntax:

Syntax
------

.. code:: syntaxbox

   #include <prclist.h>

   PRCList *PR_PREV_LINK (PRCList *elemp);

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

:ref:`PR_PREV_LINK` returns a pointer to the element preceding the
specified element. It can be used to traverse a list. The preceding
element is not removed from the list.
