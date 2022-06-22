valid-lazy
==========

Ensures that definitions and uses of properties on the ``lazy`` object are valid.
This rule checks for using unknown properties, duplicated symbols and unused
symbols.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    const lazy = {};
    // Unknown lazy member property {{name}}
    lazy.bar.foo();

.. code-block:: js

    const lazy = {};
    XPCOMUtils.defineLazyGetter(lazy, "foo", "foo.jsm");

    // Duplicate symbol foo being added to lazy.
    XPCOMUtils.defineLazyGetter(lazy, "foo", "foo1.jsm");
    lazy.foo3.bar();

.. code-block:: js

    const lazy = {};
    // Unused lazy property foo
    XPCOMUtils.defineLazyGetter(lazy, "foo", "foo.jsm");

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const lazy = {};
    XPCOMUtils.defineLazyGetter(lazy, "foo1", () => {});
    XPCOMUtils.defineLazyModuleGetters(lazy, { foo2: "foo2.jsm" });

    lazy.foo1.bar();
    lazy.foo2.bar();
