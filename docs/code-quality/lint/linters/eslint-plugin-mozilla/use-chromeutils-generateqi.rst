use-chromeutils-generateqi
==========================

Reject use of ``XPCOMUtils.generateQI`` and JS-implemented QueryInterface
methods in favor of ``ChromeUtils``.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    X.prototype.QueryInterface = XPCOMUtils.generateQI(["nsIMeh"]);
    X.prototype = { QueryInterface: XPCOMUtils.generateQI(["nsIMeh"]) };
    X.prototype = { QueryInterface: function QueryInterface(iid) {
      if (
        iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIMeh) ||
        iid.equals(nsIFlug) ||
        iid.equals(Ci.amIFoo)
      ) {
        return this;
      }
      throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
    } };


Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    X.prototype.QueryInterface = ChromeUtils.generateQI(["nsIMeh"]);
    X.prototype = { QueryInterface: ChromeUtils.generateQI(["nsIMeh"]) }
