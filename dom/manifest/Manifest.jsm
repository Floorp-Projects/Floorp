/*
 * Manifest.jsm is the top level api for managing installed web applications
 * https://www.w3.org/TR/appmanifest/
 *
 * It is used to trigger the installation of a web application via .install()
 * and to access the manifest data (including icons).
 *
 * TODO:
 *  - Persist installed manifest data to disk and keep track of which
 *    origins have installed applications
 *  - Trigger appropriate app installed events
 */

"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/Task.jsm");

const { ManifestObtainer } =
  Cu.import('resource://gre/modules/ManifestObtainer.jsm', {});
const { ManifestIcons } =
  Cu.import('resource://gre/modules/ManifestIcons.jsm', {});

function Manifest(browser) {
  this.browser = browser;
  this.data = null;
}

Manifest.prototype.install = Task.async(function* () {
  this.data = yield ManifestObtainer.browserObtainManifest(this.browser);
});

Manifest.prototype.icon = Task.async(function* (expectedSize) {
  return yield ManifestIcons.browserFetchIcon(this.browser, this.data, expectedSize);
});

Manifest.prototype.name = function () {
  return this.data.short_name || this.data.short_url;
}

this.EXPORTED_SYMBOLS = ["Manifest"]; // jshint ignore:line
