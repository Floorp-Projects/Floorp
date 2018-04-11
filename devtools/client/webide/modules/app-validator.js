/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var {Ci} = require("chrome");

const {FileUtils} = require("resource://gre/modules/FileUtils.jsm");
const Services = require("Services");
var strings = Services.strings.createBundle("chrome://devtools/locale/app-manager.properties");

function AppValidator({ type, location }) {
  this.type = type;
  this.location = location;
  this.errors = [];
  this.warnings = [];
}

AppValidator.prototype.error = function(message) {
  this.errors.push(message);
};

AppValidator.prototype.warning = function(message) {
  this.warnings.push(message);
};

AppValidator.prototype._getPackagedManifestFile = function() {
  let manifestFile = FileUtils.File(this.location);
  if (!manifestFile.exists()) {
    this.error(strings.GetStringFromName("validator.nonExistingFolder"));
    return null;
  }
  if (!manifestFile.isDirectory()) {
    this.error(strings.GetStringFromName("validator.expectProjectFolder"));
    return null;
  }

  let appManifestFile = manifestFile.clone();
  appManifestFile.append("manifest.webapp");

  let jsonManifestFile = manifestFile.clone();
  jsonManifestFile.append("manifest.json");

  let hasAppManifest = appManifestFile.exists() && appManifestFile.isFile();
  let hasJsonManifest = jsonManifestFile.exists() && jsonManifestFile.isFile();

  if (!hasAppManifest && !hasJsonManifest) {
    this.error(strings.GetStringFromName("validator.noManifestFile"));
    return null;
  }

  return hasAppManifest ? appManifestFile : jsonManifestFile;
};

AppValidator.prototype._getPackagedManifestURL = function() {
  let manifestFile = this._getPackagedManifestFile();
  if (!manifestFile) {
    return null;
  }
  return Services.io.newFileURI(manifestFile).spec;
};

AppValidator.checkManifest = function(manifestURL) {
  return new Promise((resolve, reject) => {
    let error;

    let req = new XMLHttpRequest();
    req.overrideMimeType("text/plain");

    try {
      req.open("GET", manifestURL, true);
      req.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE | Ci.nsIRequest.INHIBIT_CACHING;
    } catch (e) {
      error = strings.formatStringFromName("validator.invalidManifestURL", [manifestURL], 1);
      return reject(error);
    }

    req.onload = function() {
      let manifest = null;
      try {
        manifest = JSON.parse(req.responseText);
      } catch (e) {
        error = strings.formatStringFromName("validator.invalidManifestJSON", [e, manifestURL], 2);
        reject(error);
      }

      resolve({manifest, manifestURL});
    };

    req.onerror = function() {
      error = strings.formatStringFromName("validator.noAccessManifestURL", [req.statusText, manifestURL], 2);
      reject(error);
    };

    try {
      req.send(null);
    } catch (e) {
      error = strings.formatStringFromName("validator.noAccessManifestURL", [e, manifestURL], 2);
      reject(error);
    }
  });
};

AppValidator.findManifestAtOrigin = function(manifestURL) {
  let fixedManifest = Services.io.newURI(manifestURL).prePath + "/manifest.webapp";
  return AppValidator.checkManifest(fixedManifest);
};

AppValidator.findManifestPath = function(manifestURL) {
  return new Promise((resolve, reject) => {
    if (manifestURL.endsWith("manifest.webapp")) {
      reject();
    } else {
      let fixedManifest = manifestURL + "/manifest.webapp";
      resolve(AppValidator.checkManifest(fixedManifest));
    }
  });
};

AppValidator.checkAlternateManifest = function(manifestURL) {
  return (async function() {
    let result;
    try {
      result = await AppValidator.findManifestPath(manifestURL);
    } catch (e) {
      result = await AppValidator.findManifestAtOrigin(manifestURL);
    }

    return result;
  })();
};

AppValidator.prototype._fetchManifest = function(manifestURL) {
  return new Promise(resolve => {
    this.manifestURL = manifestURL;

    AppValidator.checkManifest(manifestURL)
                .then(({manifest, manifestURL}) => {
                  resolve(manifest);
                }, error => {
                  AppValidator.checkAlternateManifest(manifestURL)
                              .then(({manifest, manifestURL}) => {
                                this.manifestURL = manifestURL;
                                resolve(manifest);
                              }, () => {
                                this.error(error);
                                resolve(null);
                              });
                });
  });
};

AppValidator.prototype._getManifest = function() {
  let manifestURL;
  if (this.type == "packaged") {
    manifestURL = this._getPackagedManifestURL();
    if (!manifestURL) {
      return Promise.resolve(null);
    }
  } else if (this.type == "hosted") {
    manifestURL = this.location;
    try {
      Services.io.newURI(manifestURL);
    } catch (e) {
      this.error(strings.formatStringFromName("validator.invalidHostedManifestURL", [manifestURL, e.message], 2));
      return Promise.resolve(null);
    }
  } else {
    this.error(strings.formatStringFromName("validator.invalidProjectType", [this.type], 1));
    return Promise.resolve(null);
  }
  return this._fetchManifest(manifestURL);
};

AppValidator.prototype.validateManifest = function(manifest) {
  if (!manifest.name) {
    this.error(strings.GetStringFromName("validator.missNameManifestProperty"));
  }

  if (!manifest.icons || Object.keys(manifest.icons).length === 0) {
    this.warning(strings.GetStringFromName("validator.missIconsManifestProperty"));
  } else if (!manifest.icons["128"]) {
    this.warning(strings.GetStringFromName("validator.missIconMarketplace2"));
  }
};

AppValidator.prototype._getOriginURL = function() {
  if (this.type == "packaged") {
    let manifestURL = Services.io.newURI(this.manifestURL);
    return Services.io.newURI(".", null, manifestURL).spec;
  } else if (this.type == "hosted") {
    return Services.io.newURI(this.location).prePath;
  }
};

AppValidator.prototype.validateLaunchPath = function(manifest) {
  return new Promise(resolve => {
    // The launch_path field has to start with a `/`
    if (manifest.launch_path && manifest.launch_path[0] !== "/") {
      this.error(strings.formatStringFromName("validator.nonAbsoluteLaunchPath", [manifest.launch_path], 1));
      resolve();
    }
    let origin = this._getOriginURL();
    let path;
    if (this.type == "packaged") {
      path = "." + (manifest.launch_path || "/index.html");
    } else if (this.type == "hosted") {
      path = manifest.launch_path || "/";
    }
    let indexURL;
    try {
      indexURL = Services.io.newURI(path, null, Services.io.newURI(origin)).spec;
    } catch (e) {
      this.error(strings.formatStringFromName("validator.accessFailedLaunchPath", [origin + path], 1));
      return resolve();
    }

    let req = new XMLHttpRequest();
    req.overrideMimeType("text/plain");
    try {
      req.open("HEAD", indexURL, true);
      req.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE | Ci.nsIRequest.INHIBIT_CACHING;
    } catch (e) {
      this.error(strings.formatStringFromName("validator.accessFailedLaunchPath", [indexURL], 1));
      return resolve();
    }
    req.onload = () => {
      if (req.status >= 400) {
        this.error(strings.formatStringFromName("validator.accessFailedLaunchPathBadHttpCode", [indexURL, req.status], 2));
      }
      resolve();
    };
    req.onerror = () => {
      this.error(strings.formatStringFromName("validator.accessFailedLaunchPath", [indexURL], 1));
      resolve();
    };

    try {
      req.send(null);
    } catch (e) {
      this.error(strings.formatStringFromName("validator.accessFailedLaunchPath", [indexURL], 1));
      resolve();
    }
  });
};

AppValidator.prototype.validateType = function(manifest) {
  let appType = manifest.type || "web";
  if (!["web", "privileged", "certified"].includes(appType)) {
    this.error(strings.formatStringFromName("validator.invalidAppType", [appType], 1));
  } else if (this.type == "hosted" &&
             ["certified", "privileged"].includes(appType)) {
    this.error(strings.formatStringFromName("validator.invalidHostedPriviledges", [appType], 1));
  }

  // certified app are not fully supported on the simulator
  if (appType === "certified") {
    this.warning(strings.GetStringFromName("validator.noCertifiedSupport"));
  }
};

AppValidator.prototype.validate = function() {
  this.errors = [];
  this.warnings = [];
  return this._getManifest()
    .then((manifest) => {
      if (manifest) {
        this.manifest = manifest;

        // Skip validations for add-ons
        if (manifest.role === "addon" || manifest.manifest_version) {
          return Promise.resolve();
        }

        this.validateManifest(manifest);
        this.validateType(manifest);
        return this.validateLaunchPath(manifest);
      }
    });
};

exports.AppValidator = AppValidator;
