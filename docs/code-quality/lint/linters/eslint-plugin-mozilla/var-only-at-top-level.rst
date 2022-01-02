var-only-at-top-level
=====================

Marks all var declarations that are not at the top level invalid.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    { var foo; }
    function() { var bar; }

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    var foo;
    { let foo; }
    function () { let bar; }
