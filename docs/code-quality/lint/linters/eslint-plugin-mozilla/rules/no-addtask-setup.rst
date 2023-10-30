no-addtask-setup
================

Reject using ``add_task(async function setup() { ... })`` in favour of
``add_setup(async function() { ... })``.

Using semantically separate setup functions makes ``.only`` work correctly
and will allow for future improvements to setup/cleanup abstractions.

This option can be autofixed (``--fix``).

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    add_task(async function setup() { ... });
    add_task(function setup() { ... });
    add_task(function init() { ... });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    add_setup(async function() { ... });
    add_setup(function() { ... });
