valid-services
==============

Ensures that accesses of the ``Services`` object are valid.

Examples of incorrect code for this rule:
-----------------------------------------

Assuming ``foo`` is not defined within Services.

.. code-block:: js

    Services.foo.fn();

Examples of correct code for this rule:
---------------------------------------

Assuming ``bar`` is defined within Services.

.. code-block:: js

    Services.bar.fn();
