/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, Constructor: CC} = Components;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(req, res) {
  try {
    reallyHandleRequest(req, res);
  } catch (ex) {
    res.setStatusLine("1.0", 200, "AlmostOK");
    let msg = "Error handling request: " + ex + "\n" + ex.stack;
    log(msg);
    res.write(msg);
  }
}

function log(msg) {
  dump("YANDEX-SERVER-MOCK: " + msg + "\n");
}

const statusCodes = {
  400: "Bad Request",
  401: "Invalid API key",
  402: "This API key has been blocked",
  403: "Daily limit for requests reached",
  404: "Daily limit for chars reached",
  413: "The text size exceeds the maximum",
  422: "The text could not be translated",
  500: "Internal Server Error",
  501: "The specified translation direction is not supported",
  503: "Service Unavailable"
};

function HTTPError(code = 500, message) {
  this.code = code;
  this.name = statusCodes[code] || "HTTPError";
  this.message = message || this.name;
}
HTTPError.prototype = new Error();
HTTPError.prototype.constructor = HTTPError;

function sendError(res, err) {
  if (!(err instanceof HTTPError)) {
    err = new HTTPError(typeof err == "number" ? err : 500,
                        err.message || typeof err == "string" ? err : "");
  }
  res.setStatusLine("1.1", err.code, err.name);
  res.write(err.message);
}

// Based on the code borrowed from:
// http://stackoverflow.com/questions/901115/how-can-i-get-query-string-values-in-javascript
function parseQuery(query) {
  let match,
      params = {},
      pl     = /\+/g,
      search = /([^&=]+)=?([^&]*)/g,
      decode = function (s) { return decodeURIComponent(s.replace(pl, " ")); };

  while (match = search.exec(query)) {
    let k = decode(match[1]),
        v = decode(match[2]);
    if (k in params) {
      if(params[k] instanceof Array)
        params[k].push(v);
      else
        params[k] = [params[k], v];
    } else {
      params[k] = v;
    }
  }

  return params;
}

function sha1(str) {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  // `result` is an out parameter, `result.value` will contain the array length.
  let result = {};
  // `data` is an array of bytes.
  let data = converter.convertToByteArray(str, result);
  let ch = Cc["@mozilla.org/security/hash;1"]
             .createInstance(Ci.nsICryptoHash);
  ch.init(ch.SHA1);
  ch.update(data, data.length);
  let hash = ch.finish(false);

  // Return the two-digit hexadecimal code for a byte.
  function toHexString(charCode) {
    return ("0" + charCode.toString(16)).slice(-2);
  }

  // Convert the binary hash data to a hex string.
  return Array.from(hash, (c, i) => toHexString(hash.charCodeAt(i))).join("");
}

function getRequestBody(req) {
  let avail;
  let bytes = [];
  let body = new BinaryInputStream(req.bodyInputStream);

  while ((avail = body.available()) > 0)
    Array.prototype.push.apply(bytes, body.readByteArray(avail));

  return String.fromCharCode.apply(null, bytes);
}

function getInputStream(path) {
  let file = Cc["@mozilla.org/file/directory_service;1"]
               .getService(Ci.nsIProperties)
               .get("CurWorkD", Ci.nsILocalFile);
  for (let part of path.split("/"))
    file.append(part);
  let fileStream  = Cc["@mozilla.org/network/file-input-stream;1"]
                      .createInstance(Ci.nsIFileInputStream);
  fileStream.init(file, 1, 0, false);
  return fileStream;
}

/**
 * Yandex Requests have to be signed with an API Key. This mock server
 * supports the following keys:
 *
 * yandexValidKey         Always passes the authentication,
 * yandexInvalidKey       Never passes authentication and fails with 401 code,
 * yandexBlockedKey       Never passes authentication and fails with 402 code,
 * yandexOutOfRequestsKey Never passes authentication and fails with 403 code,
 * yandexOutOfCharsKey    Never passes authentication and fails with 404 code.
 *
 * If any other key is used the server reponds with 401 error code.
 */
function checkAuth(params) {
  if(!("key" in params))
    throw new HTTPError(400);

  let key = params.key;
  if(key === "yandexValidKey")
    return true;

  let invalidKeys = {
    "yandexInvalidKey"        : 401,
    "yandexBlockedKey"        : 402,
    "yandexOutOfRequestsKey"  : 403,
    "yandexOutOfCharsKey"     : 404,
  };

  if(key in invalidKeys)
    throw new HTTPError(invalidKeys[key]);

  throw new HTTPError(401);
}

function reallyHandleRequest(req, res) {

  try {

    // Preparing the query parameters.
    let params = {};
    if(req.method == 'POST') {
      params = parseQuery(getRequestBody(req));
    }

    // Extracting the API key and attempting to authenticate the request.
    log(JSON.stringify(params));

    checkAuth(params);
    methodHandlers['translate'](res, params);

  } catch (ex) {
    sendError(res, ex, ex.code);
  }

}

const methodHandlers = {
  translate: function(res, params) {
    res.setStatusLine("1.1", 200, "OK");
    res.setHeader("Content-Type", "application/json");

    let hash = sha1(JSON.stringify(params)).substr(0, 10);
    log("SHA1 hash of content: " + hash);

    let fixture = "browser/browser/components/translation/test/fixtures/result-yandex-" + hash + ".json";
    log("PATH: " + fixture);

    let inputStream = getInputStream(fixture);
    res.bodyOutputStream.writeFrom(inputStream, inputStream.available());
    inputStream.close();
  }

};
