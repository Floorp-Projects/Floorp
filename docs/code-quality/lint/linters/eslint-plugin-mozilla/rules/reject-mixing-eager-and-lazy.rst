reject-mixing-eager-and-lazy
==================================

Rejects defining a lazy getter for a module that's eagerly imported at
top-level script unconditionally.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    const { SomeProp } =
      ChromeUtils.importESModule("resource://gre/modules/Foo.sys.mjs");
    ChromeUtils.defineESModuleGetters(lazy, {
      OtherProp: "resource://gre/modules/Foo.sys.mjs",
    });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const { SomeProp, OtherProp } = ChromeUtils.importESModule(
      "resource://gre/modules/Foo.sys.mjs"
    );
