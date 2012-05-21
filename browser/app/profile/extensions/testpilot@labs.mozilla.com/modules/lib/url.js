/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var ios = Cc['@mozilla.org/network/io-service;1']
          .getService(Ci.nsIIOService);

var resProt = ios.getProtocolHandler("resource")
              .QueryInterface(Ci.nsIResProtocolHandler);

function newURI(uriStr) {
  try {
    return ios.newURI(uriStr, null, null);
  } catch (e if e.result == Cr.NS_ERROR_MALFORMED_URI) {
    throw new Error("malformed URI: " + uriStr);
  } catch (e if (e.result == Cr.NS_ERROR_FAILURE ||
                 e.result == Cr.NS_ERROR_ILLEGAL_VALUE)) {
    throw new Error("invalid URI: " + uriStr);
  }
}

function resolveResourceURI(uri) {
  var resolved;
  try {
    resolved = resProt.resolveURI(uri);
  } catch (e if e.result == Cr.NS_ERROR_NOT_AVAILABLE) {
    throw new Error("resource does not exist: " + uri.spec);
  };
  return resolved;
}

var fromFilename = exports.fromFilename = function fromFilename(path) {
  var file = Cc['@mozilla.org/file/local;1']
             .createInstance(Ci.nsILocalFile);
  file.initWithPath(path);
  return ios.newFileURI(file).spec;
};

var toFilename = exports.toFilename = function toFilename(url) {
  var uri = newURI(url);
  if (uri.scheme == "resource")
    uri = newURI(resolveResourceURI(uri));
  if (uri.scheme == "file") {
    var file = uri.QueryInterface(Ci.nsIFileURL).file;
    return file.path;
  }
  throw new Error("cannot map to filename: " + url);
};

var parse = exports.parse = function parse(url) {
  var uri = newURI(url);

  var userPass = null;
  try {
    userPass = uri.userPass ? uri.userPass : null;
  } catch (e if e.result == Cr.NS_ERROR_FAILURE) {}

  var host = null;
  try {
    host = uri.host;
  } catch (e if e.result == Cr.NS_ERROR_FAILURE) {}

  var port = null;
  try {
    port = uri.port == -1 ? null : uri.port;
  } catch (e if e.result == Cr.NS_ERROR_FAILURE) {}

  return {scheme: uri.scheme,
          userPass: userPass,
          host: host,
          port: port,
          path: uri.path};
};

var resolve = exports.resolve = function resolve(base, relative) {
  var baseURI = newURI(base);
  return ios.newURI(relative, null, baseURI).spec;
};
