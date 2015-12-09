/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/MessageBroadcaster.jsm");

Cu.importGlobalProperties(['File']);

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PermissionsInstaller",
  "resource://gre/modules/PermissionsInstaller.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm");

this.EXPORTED_SYMBOLS = ["ImportExport"];

const kAppArchiveMimeType  = "application/openwebapp+zip";
const kAppArchiveExtension = ".wpk"; // Webapp Package
const kAppArchiveVersion   = 1;

// From prio.h
const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE    = 0x20;

function debug(aMsg) {
#ifdef DEBUG
  dump("-*- ImportExport.jsm : " + aMsg + "\n");
#endif
}

/*
The full meta-data for an app looks like the properties set by
the mozIApplication constructor in AppsUtils.jsm.  We don't export anything
that can't be recreated from the manifest or by using sane defaults.
*/

// Reads a JSON object from a zip.
function readObjectFromZip(aZipReader, aPath) {
  if (!aZipReader.hasEntry(aPath)) {
    debug("ZIP doesn't have entry " + aPath);
    return;
  }

  let istream = aZipReader.getInputStream(aPath);

  // Obtain a converter to read from a UTF-8 encoded input stream.
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";

  let res;
  try {
    res = JSON.parse(converter.ConvertToUnicode(
      NetUtil.readInputStreamToString(istream, istream.available()) || ""));
  } catch(e) {
    debug("error reading " + aPath + " from ZIP: " + e);
  }
  return res;
}

this.ImportExport = {
  getUUID: function() {
    let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                          .getService(Ci.nsIUUIDGenerator);
    return uuidGenerator.generateUUID().toString();
  },

  // Exports to a Blob. Returns a promise that is resolved with the Blob and
  // rejected with an error string.
  // Possible errors are:
  // NoSuchApp, AppNotFullyInstalled, CertifiedAppExportForbidden, ZipWriteError,
  // NoAppDirectory
  export: Task.async(function*(aApp) {
    if (!aApp) {
      // Should not happen!
      throw "NoSuchApp";
    }

    debug("Exporting " + aApp.manifestURL);

    if (aApp.installState != "installed") {
      throw "AppNotFullyInstalled";
    }

    // Exporting certified apps is forbidden, as it is to import them.
    // We *have* to do this check in the parent process.
    let devMode = false;
    try {
      devMode = Services.prefs.getBoolPref("dom.apps.developer_mode");
    } catch(e) {};

    if (aApp.appStatus == Ci.nsIPrincipal.APP_STATUS_CERTIFIED && !devMode) {
      throw "CertifiedAppExportForbidden";
    }

    // Add the metadata we'll need to recreate the app object.
    let meta = {
      installOrigin: aApp.InstallOrigin,
      manifestURL: aApp.manifestURL,
      version: kAppArchiveVersion
    };

    // Add all the needed files in the app's base path to the archive.

    debug("Adding files from " + aApp.basePath + "/" + aApp.id);
    let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    dir.initWithPath(aApp.basePath);
    dir.append(aApp.id);
    if (!dir.exists() || !dir.isDirectory()) {
      throw "NoAppDirectory";
    }

    let files = [];
    debug("aApp=" + uneval(aApp));
    if (aApp.origin.startsWith("app://")) {
      // Apps sideloaded from WebIDE don't have an update manifest.
      if (!aApp.sideloaded) {
        files.push("update.webapp");
      }
      files.push("application.zip");
    } else {
      files.push("manifest.webapp");
    }

    // Creates the archive and adds the application meta-data.
    // Using the app id as the file name prevents name collisions.
    let zipWriter = Cc["@mozilla.org/zipwriter;1"]
                      .createInstance(Ci.nsIZipWriter);
    let uuid = this.getUUID();

    // We create the file in ${TempDir}/mozilla-temp-files to make sure we
    // can remove it once the blob has been used even on windows.
    // See https://mxr.mozilla.org/mozilla-central/source/xpcom/io/nsAnonymousTemporaryFile.cpp?rev=6c1c7e45c902#127
    let zipFile = FileUtils.getFile("TmpD",
      ["mozilla-temp-files", uuid + kAppArchiveExtension]);
    debug("Creating archive " + zipFile.path);

    zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

    let blob;

    try {
      debug("Adding metadata.json to exported blob.");
      let stream = Cc["@mozilla.org/io/string-input-stream;1"]
                     .createInstance(Ci.nsIStringInputStream);
      let s = JSON.stringify(meta);
      stream.setData(s, s.length);
      zipWriter.addEntryStream("metadata.json", Date.now(),
                               Ci.nsIZipWriter.COMPRESSION_DEFAULT,
                               stream, false);

      files.forEach((aName) => {
        let file = dir.clone();
        file.append(aName);
        debug("Adding " + file.leafName + " to export blob.");
        zipWriter.addEntryFile(file.leafName,
                               Ci.nsIZipWriter.COMPRESSION_DEFAULT,
                               file, false);
      });

      zipWriter.close();
      // Reads back the file as Blob.
      // File is only available on Window and Worker objects, so we need
      // to get a window there...
      let win = Services.wm.getMostRecentWindow("navigator:browser");
      if (!win) {
        throw "NoWindowAvailable";
      }
      blob = new win.File(zipFile, { name: aApp.id + kAppArchiveExtension,
                                     type: kAppArchiveMimeType,
                                     temporary: true });
    } catch(e) {
      debug("Error: " + e);
      zipWriter.close();
      zipFile.remove(false);
      throw "ZipWriteError";
    }

    return blob;
  }),

  // Returns the manifest for this hosted app.
  _importHostedApp: function(aZipReader, aManifestURL) {
    debug("Importing hosted app " + aManifestURL);

    if (!aZipReader.hasEntry("manifest.webapp")) {
      throw "NoManifestFound";
    }

    let manifest = readObjectFromZip(aZipReader, "manifest.webapp");
    if (!manifest) {
      throw "NoManifestFound";
    }

    return manifest;
  },

  // Returns the manifest for this packaged app.
  _importPackagedApp: function(aZipReader, aManifestURL, aDir) {
    debug("Importing packaged app " + aManifestURL);

    if (!aZipReader.hasEntry("application.zip")) {
      throw "NoPackageFound";
    }

    // The order matters, application.zip needs to be the last element.
    let files = [];
    aZipReader.hasEntry("update.webapp") && files.push("update.webapp");
    files.push("application.zip");

    // Extract application.zip and update.webapp
    // We get manifest.webapp from application.zip itself.
    let file;
    files.forEach((aName) => {
      file = aDir.clone();
      file.append(aName);
      aZipReader.extract(aName, file);
    });

    // |file| now points to application.zip, open it.
    let appZipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                         .createInstance(Ci.nsIZipReader);
    try {
      appZipReader.open(file);
    } catch(e) {
      throw "InvalidZip";
    }

    if (!appZipReader.hasEntry("manifest.webapp")) {
      throw "NoManifestFound";
    }

    return [readObjectFromZip(appZipReader, "manifest.webapp"),
            readObjectFromZip(appZipReader, "update.webapp"),
            file];
  },

  // Returns a promise that resolves to the temp file path.
  _writeBlobToTempFile: function(aBlob) {
    // Save the blob to a temp file.
    debug("_writeBlobToTempFile");
    let path;
    return new Promise((aResolve, aReject) => {
      let reader = new FileReader();
      reader.onloadend = () => {
        path = OS.Path.join(OS.Constants.Path.tmpDir, "app-blob.zip");
        debug("onloadend path=" + path);
        OS.File.openUnique(path).then(obj => {
          path = obj.path;
          let file = obj.file;
          debug("openUnique path=" + path);
          return file.write(new Uint8Array(reader.result))
                 .then(file.close.bind(file))
        })
        .then(() => aResolve(path))
        .catch(aReject);
      }
      reader.readAsArrayBuffer(aBlob);
    });
  },

  // Imports a blob, returning a Promise that resolves to
  // [manifestURL, manifest]
  // Possible errors are:
  // NoBlobFound, InvalidZip, MissingMetadataFile, IncorrectVersion,
  // AppAlreadyInstalled, DontImportCertifiedApps, InvalidManifest,
  // InvalidPrivilegeLevel, InvalidOrigin, DuplicateOrigin
  import: Task.async(function*(aBlob) {

    // First, do we even have a blob?
    if (!aBlob || !aBlob instanceof Ci.nsIDOMBlob) {
      throw "NoBlobFound";
    }

    let isFileBlob = aBlob instanceof File;
    // We can't QI the DOMFile to nsIFile, so we need to create one.
    let zipFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    if (!isFileBlob) {
      let path = yield this._writeBlobToTempFile(aBlob);
      zipFile.initWithPath(path);
    } else {
      zipFile.initWithPath(aBlob.mozFullPath);
    }

    debug("Importing from " + zipFile.path);

    let meta;
    let appDir;
    let manifest;
    let updateManifest;
    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                      .createInstance(Ci.nsIZipReader);
    try {
      try {
        zipReader.open(zipFile);
      } catch(e) {
        throw "InvalidZip";
      }

      // Do some sanity checks on the metadata.json and manifest.webapp files.
      if (!zipReader.hasEntry("metadata.json")) {
        throw "MissingMetadataFile";
      }

      meta = readObjectFromZip(zipReader, "metadata.json");
      if (!meta) {
        throw "NoMetadata";
      }
      debug("metadata: " + uneval(meta));

      // Bail out if that comes from an unsupported archive version.
      if (meta.version !== 1) {
        throw "IncorrectVersion";
      }

      // Check if we already have an app installed for this manifest url.
      let app = DOMApplicationRegistry.getAppByManifestURL(meta.manifestURL);
      if (app) {
        throw "AppAlreadyInstalled";
      }

      // Create a new app id & localId.
      // TODO: stop accessing internal methods of other objects.
      meta.localId = DOMApplicationRegistry._nextLocalId();
      meta.id = this.getUUID();
      meta.basePath = FileUtils.getDir(DOMApplicationRegistry.dirKey,
                                       ["webapps"], false).path;

      appDir = FileUtils.getDir(DOMApplicationRegistry.dirKey,
                                ["webapps", meta.id], true);

      let isPackage = zipReader.hasEntry("application.zip");

      let appFile;

      if (isPackage) {
        [manifest, updateManifest, appFile] =
          this._importPackagedApp(zipReader, meta.manifestURL, appDir);
      } else {
        manifest = this._importHostedApp(zipReader, meta.manifestURL);
      }

      if (!AppsUtils.checkManifest(manifest)) {
        throw "InvalidManifest";
      }

      let manifestFile = appDir.clone();
      manifestFile.append("manifest.webapp");

      let manifestString = JSON.stringify(manifest);

      // We now have the correct manifest. Save it.
      // TODO: stop accessing internal methods of other objects.
      yield DOMApplicationRegistry._writeFile(manifestFile.path,
                                              manifestString)
      .then(() => { debug("Manifest saved."); },
            aError => { debug("Error saving manifest: " + aError )});

      // Default values for the common fields.
      // TODO: share this code with the main install flow and with
      // DOMApplicationRegistry::addInstalledApp
      meta.name = manifest.name;
      meta.csp = manifest.csp;
      meta.installTime = Date.now();
      meta.removable = true;
      meta.progress = 0.0;
      meta.installState = "installed";
      meta.downloadAvailable = false;
      meta.downloading = false;
      meta.readyToApplyDownload = false;
      meta.downloadSize = 0;
      meta.lastUpdateCheck = Date.now();
      meta.updateTime = Date.now();
      meta.manifestHash = AppsUtils.computeHash(manifestString);
      meta.installerAppId = Ci.nsIScriptSecurityManager.NO_APP_ID;
      meta.installerIsBrowser = false;
      meta.role = manifest.role;

      // If there is an id in the mini-manifest, use it for blocklisting purposes.
      if (isPackage && updateManifest && ("id" in updateManifest)) {
        meta.blocklistId = updateManifest["id"];
      }

      let devMode = false;
      try {
        devMode = Services.prefs.getBoolPref("dom.apps.developer_mode");
      } catch(e) {}

      // Set the appropriate metadata for hosted and packaged apps.
      if (isPackage) {
        meta.origin = "app://" + meta.id;
        // Signature check
        // TODO: stop accessing internal methods of other objects.
        let [reader, isSigned] =
          yield DOMApplicationRegistry._openPackage(appFile, meta, false);
        let maxStatus = isSigned ? Ci.nsIPrincipal.APP_STATUS_PRIVILEGED
                                 : Ci.nsIPrincipal.APP_STATUS_INSTALLED;
        // Anything is possible in developer mode.
        if (devMode) {
          maxStatus = Ci.nsIPrincipal.APP_STATUS_CERTIFIED;
        }
        meta.appStatus = AppsUtils.getAppManifestStatus(manifest);
        debug("Signed app? " + isSigned);
        if (meta.appStatus > maxStatus) {
          throw "InvalidPrivilegeLevel";
        }

        // Custom origin.
        // We unfortunately can't reuse _checkOrigin here.
        if ((isSigned || devMode) &&
            meta.appStatus >= Ci.nsIPrincipal.APP_STATUS_PRIVILEGED &&
            manifest.origin) {
          let uri;
          try {
            uri = Services.io.newURI(manifest.origin, null, null);
          } catch(e) {
            throw "InvalidOrigin";
          }
          if (uri.scheme != "app") {
            throw "InvalidOrigin";
          }
          meta.id = uri.prePath.substring(6); // "app://".length
          if (meta.id in DOMApplicationRegistry.webapps) {
            throw "DuplicateOrigin";
          }
          // We need to change the app directory name to match the new meta.id
          appDir.moveTo(appDir.parent, meta.id);

          meta.origin = uri.prePath;
        }
      } else {
        let uri = Services.io.newURI(meta.manifestURL, null, null);
        meta.origin = uri.resolve("/");
        meta.appStatus = Ci.nsIPrincipal.APP_STATUS_INSTALLED;
        if (manifest.appcache_path) {
          // We don't export the content of the appcache, so set the app
          // in the state that will trigger download.
          meta.installState = "pending";
          meta.downloadAvailable = true;
        }
      }
      meta.kind = DOMApplicationRegistry.appKind(meta, manifest);

      DOMApplicationRegistry.webapps[meta.id] = meta;

      // Set permissions and handlers
      PermissionsInstaller.installPermissions(
        {
          origin: meta.origin,
          manifestURL: meta.manifestURL,
          manifest: manifest
        },
        false,
        null);
      DOMApplicationRegistry.updateAppHandlers(null /* old manifest */,
                                               manifest,
                                               meta);

      // Save the app registry, and sends the various notifications.
      // TODO: stop accessing internal methods of other objects.
      yield DOMApplicationRegistry._saveApps();

      app = AppsUtils.cloneAppObject(meta);
      app.manifest = manifest;
      MessageBroadcaster.broadcastMessage("Webapps:AddApp",
                                          { id: meta.id, app: app });
      MessageBroadcaster.broadcastMessage("Webapps:Install:Return:OK",
                                          { app: app });
      Services.obs.notifyObservers(null, "webapps-installed",
        JSON.stringify({ manifestURL: meta.manifestURL }));

    } catch(e) {
      debug("Import failed: " + e);
      if (appDir && appDir.exists()) {
        appDir.remove(true);
      }
      throw e;
    } finally {
      zipReader.close();
      if (!isFileBlob) {
        zipFile.remove(false);
      }
    }

    return [meta.manifestURL, manifest];
  }),

  // Extracts the manifest from a blob, returning a Promise that resolves to
  // the manifest
  // Possible errors are:
  // NoBlobFound, InvalidZip, MissingMetadataFile.
  extractManifest: Task.async(function*(aBlob) {
    // First, do we even have a blob?
    if (!aBlob || !aBlob instanceof Ci.nsIDOMBlob) {
      throw "NoBlobFound";
    }

    let isFileBlob = aBlob instanceof File;
    // We can't QI the DOMFile to nsIFile, so we need to create one.
    let zipFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    if (!isFileBlob) {
      let path = yield this._writeBlobToTempFile(aBlob);
      zipFile.initWithPath(path);
    } else {
      zipFile.initWithPath(aBlob.mozFullPath);
    }

    debug("extractManifest from " + zipFile.path);

    // Do some sanity checks on the metadata.json and manifest.webapp files.
    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                      .createInstance(Ci.nsIZipReader);

    let manifest;
    try {
      try {
        zipReader.open(zipFile);
      } catch(e) {
        throw "InvalidZip";
      }

      if (zipReader.hasEntry("manifest.webapp")) {
        manifest = readObjectFromZip(zipReader, "manifest.webapp");
        if (!manifest) {
          throw "NoManifest";
        }
      } else if (zipReader.hasEntry("application.zip")) {
        // That's a packaged app, we need to extract from the inner zip.
        let innerReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                            .createInstance(Ci.nsIZipReader);
        innerReader.openInner(zipReader, "application.zip");
        manifest = readObjectFromZip(innerReader, "manifest.webapp");
        innerReader.close();
        if (!manifest) {
          throw "NoManifest";
        }
      } else {
        throw "MissingManifestFile";
      }
    } finally {
      zipReader.close();
      if (!isFileBlob) {
        zipFile.remove(false);
      }
    }

    return manifest;
  }),
};
