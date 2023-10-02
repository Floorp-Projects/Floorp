use-chromeutils-import
======================

Require use of ``ChromeUtils.import`` rather than ``Components.utils.import``.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    Components.utils.import("resource://gre/modules/AppConstants.jsm", this);

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    ChromeUtils.import("resource://gre/modules/AppConstants.jsm", this);
    ChromeUtils.defineModuleGetter(this, "AppConstants", "resource://gre/modules/AppConstants.jsm");
