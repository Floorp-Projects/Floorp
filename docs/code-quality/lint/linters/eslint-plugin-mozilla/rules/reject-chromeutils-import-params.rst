reject-chromeutils-import-params
================================

``ChromeUtils.import`` used to be able to be called with two arguments, however
the second argument is no longer supported. Exports from modules should now be
explicit, and the imported symbols being accessed from the returned object.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    ChromeUtils.import("resource://gre/modules/AppConstants.jsm", this);
    ChromeUtils.import("resource://gre/modules/AppConstants.jsm", null);
    ChromeUtils.import("resource://gre/modules/AppConstants.jsm", {});

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
