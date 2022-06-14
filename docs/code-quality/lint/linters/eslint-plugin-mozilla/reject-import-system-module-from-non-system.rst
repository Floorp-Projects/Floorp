reject-import-system-module-from-non-system
===========================================

Rejects static import declaration for system modules (``.sys.mjs``) from non-system
modules.

Using static import for a system module into a non-system module would create a separate instance of the imported object(s) that is not shared with the other system modules and would break the per-process singleton expectation.

The reason for this is that inside system modules, a static import will load the module into the shared global. Inside non-system modules, the static import will load into a different global (e.g. window). This will cause the module to be loaded into different scopes, and hence create separate instances. The fix is to use ``ChromeUtils.importESM`` which will import the object via the system module shared global scope.


Examples of incorrect code for this rule:
-----------------------------------------

Inside a non-system module:

.. code-block:: js

    import { Services } from "resource://gre/modules/Services.sys.mjs";

Examples of correct code for this rule:
---------------------------------------

Inside a non-system module:

.. code-block:: js

    const { Services } = ChromeUtils.importESM(
      "resource://gre/modules/Services.sys.mjs"
    );

Inside a system module:

.. code-block:: js

    import { Services } from "resource://gre/modules/Services.sys.mjs";
