"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Extension",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionData",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

/* exported normalizeManifest */

let BASE_MANIFEST = {
  "applications": {"gecko": {"id": "test@web.ext"}},

  "manifest_version": 2,

  "name": "name",
  "version": "0",
};

function* normalizeManifest(manifest, baseManifest = BASE_MANIFEST) {
  const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});
  yield Management.lazyInit();

  let errors = [];
  let context = {
    url: null,

    logError: error => {
      errors.push(error);
    },

    preprocessors: {},
  };

  manifest = Object.assign({}, baseManifest, manifest);

  let normalized = Schemas.normalize(manifest, "manifest.WebExtensionManifest", context);
  normalized.errors = errors;

  return normalized;
}
