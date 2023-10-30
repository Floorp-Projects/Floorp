prefer-boolean-length-check
===========================

Prefers using a boolean length check rather than comparing against zero.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    if (foo.length == 0) {}
    if (foo.length > 0) {}
    if (foo && foo.length == 0) {}
    function bar() { return foo.length > 0 }

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    if (foo.length && foo.length) {}
    if (!foo.length) {}
    var a = foo.length > 0
    function bar() { return !!foo.length }
