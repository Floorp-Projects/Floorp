/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
