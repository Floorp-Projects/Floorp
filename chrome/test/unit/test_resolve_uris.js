/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// head_crtestutils.js doesn't get included in the child by default
if (typeof registerManifests === "undefined") {
  load("../unit/head_crtestutils.js");
}

var manifestFile = do_get_file("../unit/data/test_resolve_uris.manifest");

var manifests = [ manifestFile ];
registerManifests(manifests);

var ios = Cc["@mozilla.org/network/io-service;1"].
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
