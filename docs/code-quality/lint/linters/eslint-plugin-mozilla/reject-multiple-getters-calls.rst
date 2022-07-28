reject-multiple-getters-calls
=============================

Rejects multiple calls on ``ChromeUtils.defineESModuleGetters`` for the same
target in the same context.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
    ChromeUtils.defineESModuleGetters(lazy, {
      PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
    });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
      PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
    });
