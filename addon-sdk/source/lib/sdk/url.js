/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Cc, Ci, Cr } = require("chrome");

const { Class } = require("./core/heritage");
const base64 = require("./base64");
var tlds = Cc["@mozilla.org/network/effective-tld-service;1"]
          .getService(Ci.nsIEffectiveTLDService);

var ios = Cc['@mozilla.org/network/io-service;1']
          .getService(Ci.nsIIOService);

var resProt = ios.getProtocolHandler("resource")
              .QueryInterface(Ci.nsIResProtocolHandler);

var URLParser = Cc["@mozilla.org/network/url-parser;1?auth=no"]
                .getService(Ci.nsIURLParser);

function newURI(uriStr, base) {
  try {
    let baseURI = base ? ios.newURI(base, null, null) : null;
    return ios.newURI(uriStr, null, baseURI);
  }
  catch (e) {
    if (e.result == Cr.NS_ERROR_MALFORMED_URI) {
      throw new Error("malformed URI: " + uriStr);
    }
    if (e.result == Cr.NS_ERROR_FAILURE ||
        e.result == Cr.NS_ERROR_ILLEGAL_VALUE) {
      throw new Error("invalid URI: " + uriStr);
    }
  }
}

function resolveResourceURI(uri) {
  var resolved;
  try {
    resolved = resProt.resolveURI(uri);
  }
  catch (e) {
    if (e.result == Cr.NS_ERROR_NOT_AVAILABLE) {
      throw new Error("resource does not exist: " + uri.spec);
    }
  }
  return resolved;
}

let fromFilename = exports.fromFilename = function fromFilename(path) {
  var file = Cc['@mozilla.org/file/local;1']
             .createInstance(Ci.nsILocalFile);
  file.initWithPath(path);
  return ios.newFileURI(file).spec;
};

let toFilename = exports.toFilename = function toFilename(url) {
  var uri = newURI(url);
  if (uri.scheme == "resource")
    uri = newURI(resolveResourceURI(uri));
  if (uri.scheme == "chrome") {
    var channel = ios.newChannelFromURI(uri);
    try {
      channel = channel.QueryInterface(Ci.nsIFileChannel);
      return channel.file.path;
    }
    catch (e) {
      if (e.result == Cr.NS_NOINTERFACE) {
        throw new Error("chrome url isn't on filesystem: " + url);
      }
    }
  }
  if (uri.scheme == "file") {
    var file = uri.QueryInterface(Ci.nsIFileURL).file;
    return file.path;
  }
  throw new Error("cannot map to filename: " + url);
};

function URL(url, base) {
  if (!(this instanceof URL)) {
     return new URL(url, base);
  }

  var uri = newURI(url, base);

  var userPass = null;
  try {
    userPass = uri.userPass ? uri.userPass : null;
  }
  catch (e) {
    if (e.result != Cr.NS_ERROR_FAILURE) {
      throw e;
    }
  }

  var host = null;
  try {
    host = uri.host;
  }
  catch (e) {
    if (e.result != Cr.NS_ERROR_FAILURE) {
      throw e;
    }
  }

  var port = null;
  try {
    port = uri.port == -1 ? null : uri.port;
  }
  catch (e) {
    if (e.result != Cr.NS_ERROR_FAILURE) {
      throw e;
    }
  }

  let uriData = [uri.path, uri.path.length, {}, {}, {}, {}, {}, {}];
  URLParser.parsePath.apply(URLParser, uriData);
  let [{ value: filepathPos }, { value: filepathLen },
    { value: queryPos }, { value: queryLen },
    { value: refPos }, { value: refLen }] = uriData.slice(2);

  let hash = uri.ref ? "#" + uri.ref : "";
  let pathname = uri.path.substr(filepathPos, filepathLen);
  let search = uri.path.substr(queryPos, queryLen);
  search = search ? "?" + search : "";

  this.__defineGetter__("scheme", function() uri.scheme);
  this.__defineGetter__("userPass", function() userPass);
  this.__defineGetter__("host", function() host);
  this.__defineGetter__("hostname", function() host);
  this.__defineGetter__("port", function() port);
  this.__defineGetter__("path", function() uri.path);
  this.__defineGetter__("pathname", function() pathname);
  this.__defineGetter__("hash", function() hash);
  this.__defineGetter__("href", function() uri.spec);
  this.__defineGetter__("origin", function() uri.prePath);
  this.__defineGetter__("protocol", function() uri.scheme + ":");
  this.__defineGetter__("search", function() search);

  Object.defineProperties(this, {
    toString: {
      value: function URL_toString() new String(uri.spec).toString(),
      enumerable: false
    },
    valueOf: {
      value: function() new String(uri.spec).valueOf(),
      enumerable: false
    },
    toSource: {
      value: function() new String(uri.spec).toSource(),
      enumerable: false
    }
  });

  return this;
};

URL.prototype = Object.create(String.prototype);
exports.URL = URL;

/**
 * Parse and serialize a Data URL.
 *
 * See: http://tools.ietf.org/html/rfc2397
 *
 * Note: Could be extended in the future to decode / encode automatically binary
 * data.
 */
const DataURL = Class({

  get base64 () {
    return "base64" in this.parameters;
  },

  set base64 (value) {
    if (value)
      this.parameters["base64"] = "";
    else
      delete this.parameters["base64"];
  },
  /**
  * Initialize the Data URL object. If a uri is given, it will be parsed.
  *
  * @param {String} [uri] The uri to parse
  *
  * @throws {URIError} if the Data URL is malformed
   */
  initialize: function(uri) {
    // Due to bug 751834 it is not possible document and define these
    // properties in the prototype.

    /**
     * An hashmap that contains the parameters of the Data URL. By default is
     * empty, that accordingly to RFC is equivalent to {"charset" : "US-ASCII"}
     */
    this.parameters = {};

    /**
     * The MIME type of the data. By default is empty, that accordingly to RFC
     * is equivalent to "text/plain"
     */
    this.mimeType = "";

    /**
     * The string that represent the data in the Data URL
     */
    this.data = "";

    if (typeof uri === "undefined")
      return;

    uri = String(uri);

    let matches = uri.match(/^data:([^,]*),(.*)$/i);

    if (!matches)
      throw new URIError("Malformed Data URL: " + uri);

    let mediaType = matches[1].trim();

    this.data = decodeURIComponent(matches[2].trim());

    if (!mediaType)
      return;

    let parametersList = mediaType.split(";");

    this.mimeType = parametersList.shift().trim();

    for (let parameter, i = 0; parameter = parametersList[i++];) {
      let pairs = parameter.split("=");
      let name = pairs[0].trim();
      let value = pairs.length > 1 ? decodeURIComponent(pairs[1].trim()) : "";

      this.parameters[name] = value;
    }

    if (this.base64)
      this.data = base64.decode(this.data);

  },

  /**
   * Returns the object as a valid Data URL string
   *
   * @returns {String} The Data URL
   */
  toString : function() {
    let parametersList = [];

    for (let name in this.parameters) {
      let encodedParameter = encodeURIComponent(name);
      let value = this.parameters[name];

      if (value)
        encodedParameter += "=" + encodeURIComponent(value);

      parametersList.push(encodedParameter);
    }

    // If there is at least a parameter, add an empty string in order
    // to start with a `;` on join call.
    if (parametersList.length > 0)
      parametersList.unshift("");

    let data = this.base64 ? base64.encode(this.data) : this.data;

    return "data:" +
      this.mimeType +
      parametersList.join(";") + "," +
      encodeURIComponent(data);
  }
});

exports.DataURL = DataURL;

let getTLD = exports.getTLD = function getTLD (url) {
  let uri = newURI(url.toString());
  let tld = null;
  try {
    tld = tlds.getPublicSuffix(uri);
  }
  catch (e) {
    if (e.result != Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS &&
        e.result != Cr.NS_ERROR_HOST_IS_IP_ADDRESS) {
      throw e;
    }
  }
  return tld;
};

let isValidURI = exports.isValidURI = function (uri) {
  try {
    newURI(uri);
  }
  catch(e) {
    return false;
  }
  return true;
}

function isLocalURL(url) {
  if (String.indexOf(url, './') === 0)
    return true;

  try {
    return ['resource', 'data', 'chrome'].indexOf(URL(url).scheme) > -1;
  }
  catch(e) {}

  return false;
}
exports.isLocalURL = isLocalURL;
