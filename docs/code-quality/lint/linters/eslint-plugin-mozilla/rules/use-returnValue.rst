use-returnValue
===============

Warn when idempotent methods are called and their return value is unused.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    foo.concat(bar)
    baz.concat()

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    a = foo.concat(bar)
    b = baz.concat()
