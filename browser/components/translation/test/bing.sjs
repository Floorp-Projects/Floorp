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
  // dump("BING-SERVER-MOCK: " + msg + "\n");
}

const statusCodes = {
  400: "Bad Request",
  401: "Unauthorized",
  403: "Forbidden",
  404: "Not Found",
  405: "Method Not Allowed",
  500: "Internal Server Error",
  501: "Not Implemented",
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

function parseQuery(query) {
  let ret = {};
  for (let param of query.replace(/^[?&]/, "").split("&")) {
    param = param.split("=");
    if (!param[0])
      continue;
    ret[unescape(param[0])] = unescape(param[1]);
  }
  return ret;
}

function getRequestBody(req) {
  let avail;
  let bytes = [];
  let body = new BinaryInputStream(req.bodyInputStream);

  while ((avail = body.available()) > 0)
    Array.prototype.push.apply(bytes, body.readByteArray(avail));

  return String.fromCharCode.apply(null, bytes);
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

function parseXml(body) {
  let DOMParser = Cc["@mozilla.org/xmlextras/domparser;1"]
                    .createInstance(Ci.nsIDOMParser);
  let xml = DOMParser.parseFromString(body, "text/xml");
  if (xml.documentElement.localName == "parsererror")
    throw new Error("Invalid XML");
  return xml;
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

function checkAuth(req) {
  let err = new Error("Authorization failed");
  err.code = 401;

  if (!req.hasHeader("Authorization"))
    throw new HTTPError(401, "No Authorization header provided.");

  let auth = req.getHeader("Authorization");
  if (!auth.startsWith("Bearer "))
    throw new HTTPError(401, "Invalid Authorization header content: '" + auth + "'");

  // Rejecting inactive subscriptions.
  if (auth.includes("inactive")) {
    const INACTIVE_STATE_RESPONSE = "<html><body><h1>TranslateApiException</h1><p>Method: TranslateArray()</p><p>Message: The Azure Market Place Translator Subscription associated with the request credentials is not in an active state.</p><code></code><p>message id=5641.V2_Rest.TranslateArray.48CC6470</p></body></html>";
    throw new HTTPError(401, INACTIVE_STATE_RESPONSE);
  }

}

function reallyHandleRequest(req, res) {
  log("method: " + req.method);
  if (req.method != "POST") {
    sendError(res, "Bing only deals with POST requests, not '" + req.method + "'.");
    return;
  }

  let body = getRequestBody(req);
  log("body: " + body);

  // First, we'll see if we're dealing with an XML body:
  let contentType = req.hasHeader("Content-Type") ? req.getHeader("Content-Type") : null;
  log("contentType: " + contentType);

  if (contentType.startsWith("text/xml")) {
    try {
      // For all these requests the client needs to supply the correct
      // authentication headers.
      checkAuth(req);

      let xml = parseXml(body);
      let method = xml.documentElement.localName;
      log("invoking method: " + method);
      // If the requested method is supported, delegate it to its handler.
      if (methodHandlers[method])
        methodHandlers[method](res, xml);
      else
        throw new HTTPError(501);
    } catch (ex) {
      sendError(res, ex, ex.code);
    }
  } else {
    // Not XML, so it must be a query-string.
    let params = parseQuery(body);

    // Delegate an authentication request to the correct handler.
    if ("grant_type" in params && params.grant_type == "client_credentials")
      methodHandlers.authenticate(res, params);
    else
      sendError(res, 501);
  }
}

const methodHandlers = {
  authenticate: function(res, params) {
    // Validate a few required parameters.
    if (params.scope != "http://api.microsofttranslator.com") {
      sendError(res, "Invalid scope.");
      return;
    }
    if (!params.client_id) {
      sendError(res, "Missing client_id param.");
      return;
    }
    if (!params.client_secret) {
      sendError(res, "Missing client_secret param.");
      return;
    }

    // Defines the tokens for certain client ids.
    const TOKEN_MAP = {
      'testInactive'  : 'inactive',
      'testClient'    : 'test'
    };
    let token = 'test'; // Default token.
    if((params.client_id in TOKEN_MAP)){
      token = TOKEN_MAP[params.client_id];
    }
    let content = JSON.stringify({
      access_token: token,
      expires_in: 600
    });

    res.setStatusLine("1.1", 200, "OK");
    res.setHeader("Content-Length", String(content.length));
    res.setHeader("Content-Type", "application/json");
    res.write(content);
  },

  TranslateArrayRequest: function(res, xml, body) {
    let from = xml.querySelector("From").firstChild.nodeValue;
    let to = xml.querySelector("To").firstChild.nodeValue
    log("translating from '" + from + "' to '" + to + "'");

    res.setStatusLine("1.1", 200, "OK");
    res.setHeader("Content-Type", "text/xml");

    let hash = sha1(body).substr(0, 10);
    log("SHA1 hash of content: " + hash);
    let inputStream = getInputStream(
      "browser/browser/components/translation/test/fixtures/result-" + hash + ".txt");
    res.bodyOutputStream.writeFrom(inputStream, inputStream.available());
    inputStream.close();
  }
};
