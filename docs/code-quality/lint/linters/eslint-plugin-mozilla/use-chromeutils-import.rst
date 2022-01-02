use-chromeutils-import
======================

Require use of ``ChromeUtils.import`` and ``ChromeUtils.defineModuleGetter``
rather than ``Components.utils.import`` and
``XPCOMUtils.defineLazyModuleGetter``.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    Components.utils.import("resource://gre/modules/Services.jsm", this);
    XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    ChromeUtils.import("resource://gre/modules/Services.jsm", this);
    ChromeUtils.defineModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");
    // 4 argument version of defineLazyModuleGetter is allowed.
    XPCOMUtils.defineLazyModuleGetter(this, "Services","resource://gre/modules/Service.jsm","Foo");
