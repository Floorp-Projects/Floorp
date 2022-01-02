no-compare-against-boolean-literals
===================================

Checks that boolean expressions do not compare against literal values
of ``true`` or ``false``. This is to prevent overly verbose code such as
``if (isEnabled == true)`` when ``if (isEnabled)`` would suffice.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    if (foo == true) {}
    if (foo != false) {}
    if (false == foo) {}

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

	  if (!foo) {}
    if (!!foo) {}
