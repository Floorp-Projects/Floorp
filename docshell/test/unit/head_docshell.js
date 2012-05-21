/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);
var profileDir = dirSvc.get("CurProcD", Ci.nsILocalFile);
profileDir.append("test_docshell_profile");

// Register our own provider for the profile directory.
// It will return our special docshell profile directory.
var provider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    if (prop == "ProfD") {
      var retVal = dirSvc.get("CurProcD", Ci.nsILocalFile);
      retVal.append("test_docshell_profile");
      if (!retVal.exists())
        retVal.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
      return retVal;
    }
    throw Cr.NS_ERROR_FAILURE;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);

function cleanup()
{
  // we need to remove the folder that we created for the profile
  try {
    if (profileDir.exists())
      profileDir.remove(true);
  } catch (e) {
    // windows has a slight problem with sqlite databases and trying to remove
    // them to quickly after you might expect to be done with them.  Eat any
    // errors we'll get.  This should be OK because we cleanup before and after
    // each test run.
  }
}


// cleanup from any failed test runs in the past
cleanup();

// make sure we have our profile directory available to us
profileDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
