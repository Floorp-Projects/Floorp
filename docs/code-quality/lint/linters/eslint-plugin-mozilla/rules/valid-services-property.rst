valid-services-property
=======================

Ensures that accesses of properties of items accessed via the ``Services``
object are valid.

This rule requires a full build to run, and is not turned on by default. To run
this rule manually, use:

.. code-block:: console

    MOZ_OBJDIR=objdir-ff-opt ./mach eslint --rule="mozilla/valid-services-property: error" *

Examples of incorrect code for this rule:
-----------------------------------------

Assuming ``foo`` is not defined within ``Ci.nsISearchService``.

.. code-block:: js

    Services.search.foo();

Examples of correct code for this rule:
---------------------------------------

Assuming ``bar`` is defined within ``Ci.nsISearchService``.

.. code-block:: js

    Services.search.bar();
