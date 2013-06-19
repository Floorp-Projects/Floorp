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
let MutableArray = CC('@mozilla.org/array;1', 'nsIMutableArray');

const nsICache = Ci.nsICache;
const nsIApplicationCache = Ci.nsIApplicationCache;
const applicationCacheService =
  Cc['@mozilla.org/network/application-cache-service;1']
    .getService(Ci.nsIApplicationCacheService);


function debug(aMsg) {
  //dump("-*-*- OfflineCacheInstaller.jsm : " + aMsg + "\n");
}


function enableOfflineCacheForApp(origin, appId) {
  let principal = Services.scriptSecurityManager.getAppCodebasePrincipal(
                    origin, appId, false);
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
  channel.contentType = "plain/text";
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

function parseCacheLine(app, urls, line) {
  try {
    let url = Services.io.newURI(line, null, app.origin);
    urls.push(url.spec);
  } catch(e) {
    throw new Error('Unable to parse cache line: ' + line + '(' + e + ')');
  }
}

function parseFallbackLine(app, urls, namespaces, fallbacks, line) {
  let split = line.split(/[ \t]+/);
  if (split.length != 2) {
    throw new Error('Should be made of two URLs seperated with spaces')
  }
  let type = Ci.nsIApplicationCacheNamespace.NAMESPACE_FALLBACK;
  let [ namespace, fallback ] = split;

  // Prepend webapp origin in case of absolute path
  try {
    namespace = Services.io.newURI(namespace, null, app.origin).spec;
    fallback = Services.io.newURI(fallback, null, app.origin).spec;
  } catch(e) {
    throw new Error('Unable to parse fallback line: ' + line + '(' + e + ')');
  }

  namespaces.push([type, namespace, fallback]);
  fallbacks.push(fallback);
  urls.push(fallback);
}

function parseNetworkLine(namespaces, line) {
  let type = Ci.nsIApplicationCacheNamespace.NAMESPACE_BYPASS;
  if (line[0] == '*' && (line.length == 1 || line[1] == ' '
                                          || line[1] == '\t')) {
    namespaces.push([type, '', '']);
  } else {
    namespaces.push([type, namespace, '']);
  }
}

function parseAppCache(app, path, content) {
  let lines = content.split(/\r?\n/);

  let urls = [];
  let namespaces = [];
  let fallbacks = [];

  let currentSection = 'CACHE';
  for (let i = 0; i < lines.length; i++) {
    let line = lines[i];

    // Ignore comments
    if (/^#/.test(line) || !line.length)
      continue;

    // Process section headers
    if (line == 'CACHE MANIFEST')
      continue;
    if (line == 'CACHE:') {
      currentSection = 'CACHE';
      continue;
    } else if (line == 'NETWORK:') {
      currentSection = 'NETWORK';
      continue;
    } else if (line == 'FALLBACK:') {
      currentSection = 'FALLBACK';
      continue;
    }

    // Process cache, network and fallback rules
    try {
      if (currentSection == 'CACHE') {
        parseCacheLine(app, urls, line);
      } else if (currentSection == 'NETWORK') {
        parseNetworkLine(namespaces, line);
      } else if (currentSection == 'FALLBACK') {
        parseFallbackLine(app, urls, namespaces, fallbacks, line);
      }
    } catch(e) {
      throw new Error('Invalid ' + currentSection + ' line in appcache ' +
                      'manifest:\n' + e.message +
                      '\nFrom: ' + path +
                      '\nLine ' + i + ': ' + line);
    }
  }

  return {
    urls: urls,
    namespaces: namespaces,
    fallbacks: fallbacks
  };
}

function installCache(app) {
  if (!app.cachePath) {
    return;
  }

  let cacheDir = makeFile(app.cachePath)
  cacheDir.append(app.appId);
  cacheDir.append('cache');
  if (!cacheDir.exists())
    return;

  let cacheManifest = cacheDir.clone();
  cacheManifest.append('manifest.appcache');
  if (!cacheManifest.exists())
    return;

  enableOfflineCacheForApp(app.origin, app.localId);

  // Get the url for the manifest.
  let appcacheURL = app.appcache_path;

  // The group ID contains application id and 'f' for not being hosted in
  // a browser element, but a mozbrowser iframe.
  // See netwerk/cache/nsDiskCacheDeviceSQL.cpp: AppendJARIdentifier
  let groupID = appcacheURL + '#' + app.localId+ '+f';
  let applicationCache = applicationCacheService.createApplicationCache(groupID);
  applicationCache.activate();

  readFile(cacheManifest, function readAppCache(content) {
    let entries = parseAppCache(app, cacheManifest.path, content);

    entries.urls.forEach(function processCachedFile(url) {
      // Get this nsIFile from cache folder for this URL
      // We have absolute urls, so remove the origin part to locate the
      // files.
      let path = url.replace(app.origin.spec, '');
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

    let array = new MutableArray();
    entries.namespaces.forEach(function processNamespace([type, spec, data]) {
      debug('add namespace: ' + type + ' - ' + spec + ' - ' + data + '\n');
      array.appendElement(new Namespace(type, spec, data), false);
    });
    applicationCache.addNamespaces(array);

    entries.fallbacks.forEach(function processFallback(url) {
      debug('add fallback: ' + url + '\n');
      let type = nsIApplicationCache.ITEM_FALLBACK;
      applicationCache.markEntry(url, type);
    });
  });
}


// Public API

this.OfflineCacheInstaller = {
  installCache: installCache
};

