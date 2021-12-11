no-aArgs
========

Checks that function argument names don't start with lowercase 'a' followed by
a capital letter. This is to prevent the use of Hungarian notation whereby the
first letter is a prefix that indicates the type or intended use of a variable.

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
