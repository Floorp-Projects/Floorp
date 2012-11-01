/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const CC = Components.Constructor;

this.EXPORTED_SYMBOLS = ["OfflineCacheInstaller"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

let Namespace = CC('@mozilla.org/network/application-cache-namespace;1',
                   'nsIApplicationCacheNamespace',
                   'init');
let makeFile = CC('@mozilla.org/file/local;1',
                'nsIFile',
                'initWithPath');
const nsICache = Ci.nsICache;
const nsIApplicationCache = Ci.nsIApplicationCache;
const applicationCacheService =
  Cc['@mozilla.org/network/application-cache-service;1']
    .getService(Ci.nsIApplicationCacheService);


function debug(aMsg) {
  //dump("-*-*- OfflineCacheInstaller.jsm : " + aMsg + "\n");
}


function enableOfflineCacheForApp(origin, appId) {
  let originURI = Services.io.newURI(origin, null, null);
  let principal = Services.scriptSecurityManager.getAppCodebasePrincipal(
                    originURI, appId, false);
  Services.perms.addFromPrincipal(principal, 'offline-app',
                                  Ci.nsIPermissionManager.ALLOW_ACTION);
  // Prevent cache from being evicted:
  Services.perms.addFromPrincipal(principal, 'pin-app',
                                  Ci.nsIPermissionManager.ALLOW_ACTION);
}


function storeCache(applicationCache, url, file, itemType) {
  let session = Services.cache.createSession(applicationCache.clientID,
                                             nsICache.STORE_OFFLINE, true);
  session.asyncOpenCacheEntry(url, nsICache.ACCESS_WRITE, {
    onCacheEntryAvailable: function (cacheEntry, accessGranted, status) {
      cacheEntry.setMetaDataElement('request-method', 'GET');
      cacheEntry.setMetaDataElement('response-head', 'HTTP/1.1 200 OK\r\n');

      let outputStream = cacheEntry.openOutputStream(0);

      // Input-Output stream machinery in order to push nsIFile content into cache
      let inputStream = Cc['@mozilla.org/network/file-input-stream;1']
                          .createInstance(Ci.nsIFileInputStream);
      inputStream.init(file, 1, -1, null);
      let bufferedOutputStream = Cc['@mozilla.org/network/buffered-output-stream;1']
                                   .createInstance(Ci.nsIBufferedOutputStream);
      bufferedOutputStream.init(outputStream, 1024);
      bufferedOutputStream.writeFrom(inputStream, inputStream.available());
      bufferedOutputStream.flush();
      bufferedOutputStream.close();
      outputStream.close();
      inputStream.close();

      cacheEntry.markValid();
      debug (file.path + ' -> ' + url + ' (' + itemType + ')');
      applicationCache.markEntry(url, itemType);
      cacheEntry.close();
    }
  });
}

function readFile(aFile, aCallback) {
  let channel = NetUtil.newChannel(aFile);
  channel.contentType = "pain/text";
  NetUtil.asyncFetch(channel, function(aStream, aResult) {
    if (!Components.isSuccessCode(aResult)) {
      Cu.reportError("OfflineCacheInstaller: Could not read file " + aFile.path);
      if (aCallback)
        aCallback(null);
      return;
    }

    // Obtain a converter to read from a UTF-8 encoded input stream.
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                      .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let data = NetUtil.readInputStreamToString(aStream,
                                               aStream.available());
    aCallback(converter.ConvertToUnicode(data));
  });
}

this.OfflineCacheInstaller = {
  installCache: function installCache(app) {
    let cacheDir = makeFile(app.basePath)
    cacheDir.append(app.appId);
    cacheDir.append("cache");
    if (!cacheDir.exists())
      return;

    let cacheManifest = cacheDir.clone();
    cacheManifest.append("manifest.appcache");
    if (!cacheManifest.exists())
      return;

    enableOfflineCacheForApp(app.origin, app.localId);

    // Get the url for the manifest.
    let appcacheURL = app.origin + "cache/manifest.appcache";

    // The group ID contains application id and 'f' for not being hosted in
    // a browser element, but a mozbrowser iframe.
    // See netwerk/cache/nsDiskCacheDeviceSQL.cpp: AppendJARIdentifier
    let groupID = appcacheURL + '#' + app.localId+ '+f';
    let applicationCache = applicationCacheService.createApplicationCache(groupID);
    applicationCache.activate();

    readFile(cacheManifest, function (content) {
      let lines = content.split(/\r?\n/);
      // Process each manifest line, read only CACHE entries
      // (ignore NETWORK entries) and compute absolute URL for each entry
      let urls = [];
      for(let i = 0; i < lines.length; i++) {
        let line = lines[i];
        // Ignore comments
        if (/^#/.test(line) || !line.length)
          continue;
        if (line == 'CACHE MANIFEST')
          continue;
        if (line == 'CACHE:')
          continue;
        // Ignore network entries and everything that comes after
        if (line == 'NETWORK:')
          break;

        // Prepend webapp origin in case of absolute path
        if (line[0] == '/') {
          urls.push(app.origin + line.substring(1));
        // Just pass along the url, if we have one
        } else if (line.substr(0, 4) == 'http') {
          urls.push(line);
        } else {
          throw new Error('Invalid line in appcache manifest:\n' + line +
                          '\nFrom: ' + cacheManifest.path);
        }
      }
      urls.forEach(function processCachedFile(url) {
        // Get this nsIFile from cache folder for this URL
        let path = url.replace(/https?:\/\//, '');
        let file = cacheDir.clone();
        let paths = path.split('/');
        paths.forEach(file.append);

        if (!file.exists()) {
          let msg = 'File ' + file.path + ' exists in the manifest but does ' +
                    'not points to a real file.';
          throw new Error(msg);
        }

        let itemType = nsIApplicationCache.ITEM_EXPLICIT;
        storeCache(applicationCache, url, file, itemType);
      });
    });
  }
};

