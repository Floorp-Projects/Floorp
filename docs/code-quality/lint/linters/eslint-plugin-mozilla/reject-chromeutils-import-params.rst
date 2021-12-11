reject-chromeutils-import-params
================================

``ChromeUtils.import`` can be called with two arguments, however these are now
largely deprecated.

The use of object destructuring is preferred over the second parameter being
``this``.

Using explicit exports is preferred over the second parameter being ``null``.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    ChromeUtils.import("resource://gre/modules/Services.jsm", this);
    ChromeUtils.import("resource://gre/modules/Services.jsm", null);

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
