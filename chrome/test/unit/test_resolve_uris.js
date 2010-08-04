/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Chrome Registration Test Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net> (Original Author)
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

// head_crtestutils.js doesn't get included in the child by default
if (typeof registerManifests === "undefined") {
  load("../unit/head_crtestutils.js");
}

let manifestFile = do_get_file("../unit/data/test_resolve_uris.manifest");

let manifests = [ manifestFile ];
registerManifests(manifests);

let ios = Cc["@mozilla.org/network/io-service;1"].
          getService(Ci.nsIIOService);

function do_run_test()
{
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"].
           getService(Ci.nsIChromeRegistry);

  // If we don't have libxul or e10s then we don't have process separation, so
  // we don't need to worry about checking for new chrome.
  var appInfo = Cc["@mozilla.org/xre/app-info;1"];
  if (!appInfo ||
      (appInfo.getService(Ci.nsIXULRuntime).processType ==
       Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT)) {
    cr.checkForNewChrome();
  }

  // See if our various things were able to register
  let registrationTypes = [
      "content",
      "locale",
      "skin",
      "override",
      "resource",
  ];

  for (let j = 0; j < registrationTypes.length; j++) {
    let type = registrationTypes[j];
    dump("Testing type '" + type + "'\n");
    let expectedURI = "resource://foo/foo-" + type + "/";
    let sourceURI = "chrome://foo/" + type + "/";
    switch (type) {
      case "content":
        expectedURI += "foo.xul";
        break;
      case "locale":
        expectedURI += "foo.dtd";
        break;
      case "skin":
        expectedURI += "foo.css";
        break;
      case "override":
        sourceURI = "chrome://good-package/content/override-me.xul";
        expectedURI += "override-me.xul";
        break;
      case "resource":
        expectedURI = ios.newFileURI(manifestFile.parent).spec;
        sourceURI = "resource://foo/";
        break;
    };
    try {
      sourceURI = ios.newURI(sourceURI, null, null);
      let uri;
      if (type == "resource") {
        // resources go about a slightly different way than everything else
        let rph = ios.getProtocolHandler("resource").
            QueryInterface(Ci.nsIResProtocolHandler);
        uri = rph.resolveURI(sourceURI);
      }
      else {
        uri = cr.convertChromeURL(sourceURI).spec;
      }
      
      do_check_eq(expectedURI, uri);
    }
    catch (e) {
      dump(e + "\n");
      do_throw("Should have registered a handler for type '" +
               type + "'\n");
    }
  }
}

if (typeof run_test === "undefined") {
  run_test = function() {
    do_run_test();
  };
}
