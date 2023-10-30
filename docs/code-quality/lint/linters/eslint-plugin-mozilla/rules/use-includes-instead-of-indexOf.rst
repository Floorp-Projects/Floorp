use-includes-instead-of-indexOf
===============================

Use ``.includes`` instead of ``.indexOf`` to check if something is in an array
or string.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    let a = foo.indexOf(bar) >= 0;
    let a = foo.indexOf(bar) == -1;

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    let a = foo.includes(bar);
    let a = foo.indexOf(bar) > 0;
