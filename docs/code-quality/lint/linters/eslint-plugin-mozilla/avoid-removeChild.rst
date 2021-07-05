avoid-removeChild
=================

Rejects using ``element.parentNode.removeChild(element)`` when ``element.remove()``
can be used instead.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    elt.parentNode.removeChild(elt);

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

		elt.remove();
		elt.parentNode.removeChild(elt2);
