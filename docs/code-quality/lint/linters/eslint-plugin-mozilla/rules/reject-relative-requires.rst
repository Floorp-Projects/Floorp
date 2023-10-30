reject-relative-requires
========================

Rejects calls to require which use relative directories.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    require("./relative/path")
    require("../parent/folder/path")
    loader.lazyRequireGetter(this, "path", "../parent/folder/path", true)

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    require("devtools/absolute/path")
    require("resource://gre/modules/SomeModule.jsm")
    loader.lazyRequireGetter(this, "path", "devtools/absolute/path", true)
