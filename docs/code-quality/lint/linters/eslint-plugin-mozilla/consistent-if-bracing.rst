consistent-if-bracing
=====================

Checks that if/elseif/else bodies are braced consistently, so either all bodies
are braced or unbraced. Doesn't enforce either of those styles though.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    if (true) {1} else 0
    if (true) 1; else {0}
    if (true) {1} else if (true) 2; else {0}

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

		if (true) {1} else {0}
		if (false) 1; else 0
    if (true) {1} else if (true) {2} else {0}
