lazy-getter-object-name
=============================

Enforce the standard object variable name ``lazy`` for
``ChromeUtils.defineESModuleGetters``

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    const obj = {};
    ChromeUtils.defineESModuleGetters(obj, {
      Services: “resource://gre/modules/Services.sys.mjs”,
    });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const lazy = {};
    ChromeUtils.defineESModuleGetters(lazy, {
      Services: “resource://gre/modules/Services.sys.mjs”,
    });
