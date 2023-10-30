reject-mixing-eager-and-lazy
==================================

Rejects defining a lazy getter for a module that's eagerly imported at
top-level script unconditionally.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    const { SomeProp } = ChromeUtils.import("resource://gre/modules/Foo.jsm");
    ChromeUtils.defineLazyModuleGetters(lazy, {
      OtherProp: "resource://gre/modules/Foo.jsm",
    });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const { SomeProp, OtherProp } = ChromeUtils.import("resource://gre/modules/Foo.jsm");
