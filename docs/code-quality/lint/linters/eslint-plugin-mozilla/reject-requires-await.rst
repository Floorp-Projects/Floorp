reject-requires-await
=====================

`Assert.rejects` must be preceded by an `await`, otherwise the assertion
may not be completed before the test finishes, might not be caught
and might cause intermittent issues in other tests.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    Assert.rejects(myfunc(), /startup failed/, "Should reject");

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    await Assert.rejects(myfunc(), /startup failed/, "Should reject");
