reject-eager-module-in-lazy-getter
==================================

Rejects defining a lazy getter for module that's known to be loaded early in the
startup process and it is not necessary to lazy load it.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    ChromeUtils.defineESModuleGetters(lazy, {
      Services: "resource://gre/modules/Services.sys.mjs",
    });
    XPCOMUtils.defineLazyModuleGetters(lazy, {
      XPCOMUtils: "resource://gre/modules/XPCOMUtils.jsm",
    });
    XPCOMUtils.defineLazyModuleGetter(
      lazy,
      "AppConstants",
      "resource://gre/modules/AppConstants.jsm",
    });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    import { Services } from "resource://gre/modules/Services.sys.mjs";
    const { XPCOMUtils } = ChromeUtils.import(
      "resource://gre/modules/XPCOMUtils.jsm"
    );
    const { AppConstants } = ChromeUtils.import(
      "resource://gre/modules/AppConstants.jsm"
    );
