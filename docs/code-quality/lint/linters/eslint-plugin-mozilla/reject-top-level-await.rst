reject-top-level-await
======================

Rejects ``await`` at the top-level of code in modules. Top-level ``await`` is
not currently support in Gecko's component modules, so this is rejected.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    await foo;

    if (expr) {
      await foo;
    }

    for await (let x of [1, 2, 3]) { }

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    async function() { await foo; }
    async function() { for await (let x of [1, 2, 3]) { } }
