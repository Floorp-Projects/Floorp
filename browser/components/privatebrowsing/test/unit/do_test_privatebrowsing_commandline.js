/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks the browser -private command line option.

function testprivatecl() {
}

testprivatecl.prototype = {
  _arguments: ["private", "silent"],
  get length() {
    return this._arguments.length;
  },
  getArgument: function getArgument(aIndex) {
    return this._arguments[aIndex];
  },
  findFlag: function findFlag(aFlag, aCaseSensitive) {
    for (let i = 0; i < this._arguments.length; ++i)
      if (aCaseSensitive ?
          (this._arguments[i] == aFlag) :
          (this._arguments[i].toLowerCase() == aFlag.toLowerCase()))
        return i;
    return -1;
  },
  removeArguments: function removeArguments(aStart, aEnd) {
    this._arguments.splice(aStart, aEnd - aStart + 1);
  },
  handleFlag: function handleFlag (aFlag, aCaseSensitive) {
    let res = this.findFlag(aFlag, aCaseSensitive);
    if (res > -1) {
      this.removeArguments(res, res);
      return true;
    }
    return false;
  },
  handleFlagWithParam: function handleFlagWithParam(aFlag, aCaseSensitive) {
    return null;
  },
  STATE_INITIAL_LAUNCH: 0,
  STATE_REMOTE_AUTO: 1,
  STATE_REMOTE_EXPLICIT: 2,
  get state() {
    return this.STATE_INITIAL_LAUNCH;
  },
  preventDefault: false,
  workingDirectory: null,
  windowContext: null,
  resolveFile: function resolveFile (aArgument) {
    return null;
  },
  resolveURI: function resolveURI (aArgument) {
    return null;
  },
  QueryInterface: function(aIID) {
    if (!aIID.equals(Ci.nsICommandLine)
        && !aIID.equals(Ci.nsISupports))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
}

function do_test() {
  // initialization
  let pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
           getService(Ci.nsIPrivateBrowsingService).
           QueryInterface(Ci.nsIObserver);

  let testcl = new testprivatecl();

  pb.observe(testcl, "command-line-startup", null);

  let catMan = Cc["@mozilla.org/categorymanager;1"].
               getService(Ci.nsICategoryManager);
  let categories = catMan.enumerateCategory("command-line-handler");
  while (categories.hasMoreElements()) {
    let category = categories.getNext().QueryInterface(Ci.nsISupportsCString).data;
    let contractID = catMan.getCategoryEntry("command-line-handler", category);
    let handler = Cc[contractID].getService(Ci.nsICommandLineHandler);
    handler.handle(testcl);
  }

  // the private mode should be entered automatically
  do_check_true(pb.privateBrowsingEnabled);
  // and should appear as auto-started!
  do_check_true(pb.autoStarted);
  // and should be coming from the command line!
  do_check_eq(pb.lastChangedByCommandLine, true);
}
