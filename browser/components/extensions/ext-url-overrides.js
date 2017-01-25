/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

// Bug 1320736 tracks creating a generic precedence manager for handling
// multiple addons modifying the same properties, and bug 1330494 has been filed
// to track utilizing this manager for chrome_url_overrides. Until those land,
// the edge cases surrounding multiple addons using chrome_url_overrides will
// be ignored and precedence will be first come, first serve.
let overrides = {
  // A queue of extensions in line to override the newtab page (sorted oldest to newest).
  newtab: [],
};

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_chrome_url_overrides", (type, directive, extension, manifest) => {
  if (manifest.chrome_url_overrides.newtab) {
    let newtab = manifest.chrome_url_overrides.newtab;
    let url = extension.baseURI.resolve(newtab);

    // Only set the newtab URL if no other extension is overriding it.
    if (!overrides.newtab.length) {
      aboutNewTabService.newTabURL = url;
    }

    overrides.newtab.push({extension, url});
  }
});

extensions.on("shutdown", (type, extension) => {
  let i = overrides.newtab.findIndex(o => o.extension === extension);
  if (i !== -1) {
    overrides.newtab.splice(i, 1);

    if (overrides.newtab.length) {
      aboutNewTabService.newTabURL = overrides.newtab[0].url;
    } else {
      aboutNewTabService.resetNewTabURL();
    }
  }
});
/* eslint-enable mozilla/balanced-listeners */
