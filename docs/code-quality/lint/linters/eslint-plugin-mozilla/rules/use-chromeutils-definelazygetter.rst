use-chromeutils-definelazygetter
================================

Require use of ``ChromeUtils.defineLazyGetter`` rather than ``XPCOMUtils.defineLazyGetter``.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    XPCOMUtils.defineLazyGetter(lazy, "textEncoder", function () {
        return new TextEncoder();
    });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    ChromeUtils.defineLazyGetter(lazy, "textEncoder", function () {
        return new TextEncoder();
    });
