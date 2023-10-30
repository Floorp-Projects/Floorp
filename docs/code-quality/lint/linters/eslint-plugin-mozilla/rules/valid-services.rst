valid-services
==============

Ensures that accesses of the ``Services`` object are valid.
``Services`` are defined in ``tools/lint/eslint/eslint-plugin-mozilla/lib/services.json`` and can be added by copying from
``<objdir>/xpcom/components/services.json`` after a build.

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
