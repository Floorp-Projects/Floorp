lazy-getter-object-name
=============================

Enforce the standard object variable name ``lazy`` for
``ChromeUtils.defineESModuleGetters``

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    const obj = {};
    ChromeUtils.defineESModuleGetters(obj, {
      AppConstants: “resource://gre/modules/AppConstants.sys.mjs”,
    });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const lazy = {};
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: “resource://gre/modules/AppConstants.sys.mjs”,
    });
