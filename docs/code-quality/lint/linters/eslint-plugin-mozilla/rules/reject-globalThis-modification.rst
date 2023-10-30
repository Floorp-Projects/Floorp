reject-globalThis-modification
==============================

Reject any modification to ``globalThis`` inside the system modules.

``globalThis`` is the shared global inside the system modules, and modification
on it is visible from all modules, and it shouldn't be done unless it's really
necessary.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    globalThis.foo = 10;
    Object.defineProperty(globalThis, "bar", { value: 20});
    ChromeUtils.defineESModuleGetters(globalThis, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
