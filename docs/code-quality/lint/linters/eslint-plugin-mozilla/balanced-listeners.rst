balanced-listeners
==================

Checks that for every occurrence of 'addEventListener' or 'on' there is an
occurrence of 'removeEventListener' or 'off' with the same event name.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    elt.addEventListener('click', handler, false);

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

		elt.addEventListener('event', handler);
		elt.removeEventListener('event', handler);
