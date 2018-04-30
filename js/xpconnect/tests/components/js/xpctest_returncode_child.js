/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

function TestReturnCodeChild() {}
TestReturnCodeChild.prototype = {

  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI([Ci["nsIXPCTestReturnCodeChild"]]),
  contractID: "@mozilla.org/js/xpc/test/js/ReturnCodeChild;1",
  classID: Components.ID("{38dd78aa-467f-4fad-8dcf-4383a743e235}"),

  doIt(behaviour) {
    switch (behaviour) {
      case Ci.nsIXPCTestReturnCodeChild.CHILD_SHOULD_THROW:
        throw(new Error("a requested error"));
      case Ci.nsIXPCTestReturnCodeChild.CHILD_SHOULD_RETURN_SUCCESS:
        return;
      case Ci.nsIXPCTestReturnCodeChild.CHILD_SHOULD_RETURN_RESULTCODE:
        Components.returnCode = Cr.NS_ERROR_FAILURE;
        return;
      case Ci.nsIXPCTestReturnCodeChild.CHILD_SHOULD_NEST_RESULTCODES:
        // Use xpconnect to create another instance of *this* component and
        // call that.  This way we have crossed the xpconnect bridge twice.

        // We set *our* return code early - this should be what is returned
        // to our caller, even though our "inner" component will set it to
        // a different value that we will see (but our caller should not)
        Components.returnCode = Cr.NS_ERROR_UNEXPECTED;
        // call the child asking it to do the .returnCode set.
        let sub = Cc[this.contractID].createInstance(Ci.nsIXPCTestReturnCodeChild);
        let childResult = Cr.NS_OK;
        try {
          sub.doIt(Ci.nsIXPCTestReturnCodeChild.CHILD_SHOULD_RETURN_RESULTCODE);
        } catch (ex) {
          childResult = ex.result;
        }
        // write it to the console so the test can check it.
        let consoleService = Cc["@mozilla.org/consoleservice;1"]
                             .getService(Ci.nsIConsoleService);
        consoleService.logStringMessage("nested child returned " + childResult);
        return;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestReturnCodeChild]);
