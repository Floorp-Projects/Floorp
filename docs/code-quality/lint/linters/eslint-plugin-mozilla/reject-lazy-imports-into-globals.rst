reject-lazy-imports-into-globals
================================

Rejects importing lazy items into ``window`` or ``globalThis`` when in a
non-system module scope.

Importing into the ``window`` scope (or ``globalThis``) will share the imported
global with everything else in the same window. In modules, this is generally
unnecessary and undesirable because each module imports what it requires.
Additionally, sharing items via the global scope makes it more difficult for
linters to determine the available globals.

Instead, the globals should either be imported directly, or into a lazy object.
If there is a good reason for sharing the globals via the ``window`` scope, then
this rule may be disabled as long as a comment is added explaining the reasons.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    XPCOMUtils.defineLazyModuleGetter(globalThis, "foo", "foo.jsm");
    XPCOMUtils.defineLazyModuleGetter(window, "foo", "foo.jsm");
    XPCOMUtils.defineLazyGetter(globalThis, "foo", () => {});
    XPCOMUtils.defineLazyGetter(window, "foo", () => {});

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const lazy = {};
    XPCOMUtils.defineLazyGetter(lazy, "foo", () => {});
