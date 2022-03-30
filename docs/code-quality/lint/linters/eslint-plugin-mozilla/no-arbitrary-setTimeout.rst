no-arbitrary-setTimeout
=======================

Disallows setTimeout with non-zero values in tests. Using arbitrary times for
setTimeout may cause intermittent failures in tests. A value of zero is allowed
as this is letting the event stack unwind, however also consider the use
of ``TestUtils.waitForTick``.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    function(aFoo, aBar) {}
    (aFoo, aBar) => {}

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

		function(foo, bar) {}
		(foo, bar) => {})
