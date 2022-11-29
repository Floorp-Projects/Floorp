no-cu-reportError
=================

Disallows Cu.reportError. This has been deprecated and should be replaced by
console.error.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    Cu.reportError("message");
    Cu.reportError("message", stack);

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    console.error("message");
    let error = new Error("message");
    error.stack = stack;
    console.error(error);
