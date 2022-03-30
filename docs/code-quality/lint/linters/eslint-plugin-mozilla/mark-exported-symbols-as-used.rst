mark-exported-symbols-as-used
=============================

Marks variables listed in ``EXPORTED_SYMBOLS`` as used so that ``no-unused-vars``
does not complain about them.

This rule also checks that ``EXPORTED_SYMBOLS`` is not defined using ``let`` as
``let`` isn't allowed as the lexical scope may die after the script executes.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    let EXPORTED_SYMBOLS = ["foo"];

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    var EXPORTED_SYMBOLS = ["foo"];
    const EXPORTED_SYMBOLS = ["foo"];
