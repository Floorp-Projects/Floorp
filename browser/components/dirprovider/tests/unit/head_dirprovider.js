/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

var gProfD = do_get_profile();
var gDirSvc = Cc["@mozilla.org/file/directory_service;1"].
              getService(Ci.nsIProperties);
var gPrefSvc = Cc["@mozilla.org/preferences-service;1"].
               getService(Ci.nsIPrefBranch);

function writeTestFile(aParent, aName) {
  let file = aParent.clone();
  file.append(aName);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0644);
  return file;
}

