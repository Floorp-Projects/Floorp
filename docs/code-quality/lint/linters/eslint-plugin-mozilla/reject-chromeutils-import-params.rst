reject-chromeutils-import-params
================================

``ChromeUtils.import`` used to be able to be called with two arguments, however
the second argument is no longer supported. Exports from modules should now be
explicit, and the imported symbols being accessed from the returned object.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    ChromeUtils.import("resource://gre/modules/Services.jsm", this);
    ChromeUtils.import("resource://gre/modules/Services.jsm", null);
    ChromeUtils.import("resource://gre/modules/Services.jsm", {});

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
