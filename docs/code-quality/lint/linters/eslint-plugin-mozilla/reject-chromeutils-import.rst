reject-chromeutils-import
================================

While ``ChromeUtils.import`` supports the fallback to ESMified module,
in-tree files should use ``ChromeUtils.importESModule`` for ESMified modules.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

    ChromeUtils.defineModuleGetter(
      obj, "AppConstants", "resource://gre/modules/AppConstants.jsm");
    XPCOMUtils.defineLazyModuleGetters(
      obj, { AppConstants: "resource://gre/modules/AppConstants.jsm" });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    ChromeUtils.importESModule("resource://gre/modules/AppConstants.sys.mjs");

    ChromeUtils.defineESModuleGetters(
      obj, { AppConstants: "resource://gre/modules/AppConstants.sys.mjs" });
