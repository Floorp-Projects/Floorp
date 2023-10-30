use-services
============

Requires the use of ``Services`` rather than ``Cc[].getService()`` where a
service is already defined in ``Services``.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
    Components.classes["@mozilla.org/toolkit/app-startup;1"].getService(Components.interfaces.nsIAppStartup);

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    Services.wm.addListener()
    Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator)
