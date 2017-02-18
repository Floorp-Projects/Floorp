/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

// Bug 1320736 tracks creating a generic precedence manager for handling
// multiple addons modifying the same properties, and bug 1330494 has been filed
// to track utilizing this manager for chrome_url_overrides. Until those land,
// the edge cases surrounding multiple addons using chrome_url_overrides will
// be ignored and precedence will be first come, first serve.
let overrides = {
  // A queue of extensions in line to override the newtab page (sorted oldest to newest).
  newtab: [],
  // A queue of extensions in line to override the home page (sorted oldest to newest).
  home: [],
};

/**
 * Resets the specified page to its default value.
 *
 * @param {string} page The page to override. Accepted values are "newtab" and "home".
 */
function resetPage(page) {
  switch (page) {
    case "newtab":
      aboutNewTabService.resetNewTabURL();
      break;
    case "home":
      Preferences.reset("browser.startup.homepage");
      break;
    default:
      throw new Error("Unrecognized override type");
  }
}

/**
 * Overrides the specified page to the specified URL.
 *
 * @param {string} page The page to override. Accepted values are "newtab" and "home".
 * @param {string} url The resolved URL to use for the page override.
 */
function overridePage(page, url) {
  switch (page) {
    case "newtab":
      aboutNewTabService.newTabURL = url;
      break;
    case "home":
      Preferences.set("browser.startup.homepage", url);
      break;
    default:
      throw new Error("Unrecognized override type");
  }
}

/**
 * Updates the page to the URL specified by the extension next in line. If no extensions
 * are in line, the page is reset to its default value.
 *
 * @param {string} page The page to override.
 */
function updatePage(page) {
  if (overrides[page].length) {
    overridePage(page, overrides[page][0].url);
  } else {
    resetPage(page);
  }
}

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_chrome_url_overrides", (type, directive, extension, manifest) => {
  if (Object.keys(overrides).length > 1) {
    extension.manifestError("Extensions can override only one page.");
  }

  for (let page of Object.keys(overrides)) {
    if (manifest.chrome_url_overrides[page]) {
      let relativeURL = manifest.chrome_url_overrides[page];
      let url = extension.baseURI.resolve(relativeURL);
      // Store the extension ID instead of a hard reference to the extension.
      overrides[page].push({id: extension.id, url});
      updatePage(page);
      break;
    }
  }
});

extensions.on("shutdown", (type, extension) => {
  for (let page of Object.keys(overrides)) {
    let i = overrides[page].findIndex(o => o.id === extension.id);
    if (i !== -1) {
      overrides[page].splice(i, 1);
      updatePage(page);
    }
  }
});
/* eslint-enable mozilla/balanced-listeners */
