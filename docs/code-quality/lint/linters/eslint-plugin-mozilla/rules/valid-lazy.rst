valid-lazy
==========

Ensures that definitions and uses of properties on the ``lazy`` object are valid.
This rule checks for using unknown properties, duplicated symbols, unused
symbols, and also lazy getter used at top-level unconditionally.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    const lazy = {};
    if (x) {
      // Unknown lazy member property {{name}}
      lazy.bar.foo();
    }

.. code-block:: js

    const lazy = {};
    ChromeUtils.defineLazyGetter(lazy, "foo", "foo.jsm");

    // Duplicate symbol foo being added to lazy.
    ChromeUtils.defineLazyGetter(lazy, "foo", "foo1.jsm");
    if (x) {
      lazy.foo3.bar();
    }

.. code-block:: js

    const lazy = {};
    // Unused lazy property foo
    ChromeUtils.defineLazyGetter(lazy, "foo", "foo.jsm");

.. code-block:: js

    const lazy = {};
    ChromeUtils.defineLazyGetter(lazy, "foo", "foo.jsm");
    // Used at top-level unconditionally.
    lazy.foo.bar();

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const lazy = {};
    ChromeUtils.defineLazyGetter(lazy, "foo1", () => {});
    XPCOMUtils.defineLazyModuleGetters(lazy, { foo2: "foo2.jsm" });

    if (x) {
      lazy.foo1.bar();
      lazy.foo2.bar();
    }
