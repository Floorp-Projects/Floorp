/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
  // dump("GOOGLE-SERVER-MOCK: " + msg + "\n");
}

const kBasePath = "browser/browser/components/loop/test/mochitest/fixtures/";

const kStatusCodes = {
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
  this.name = kStatusCodes[code] || "HTTPError";
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

function parseQuery(query, params = {}) {
  for (let param of query.replace(/^[?&]/, "").split(/(?:&|\?)/)) {
    param = param.split("=");
    if (!param[0])
      continue;
    params[unescape(param[0])] = unescape(param[1]);
  }
  return params;
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

function checkAuth(req) {
  if (!req.hasHeader("Authorization"))
    throw new HTTPError(401, "No Authorization header provided.");

  let auth = req.getHeader("Authorization");
  if (auth != "Bearer test-token")
    throw new HTTPError(401, "Invalid Authorization header content: '" + auth + "'");
}

function reallyHandleRequest(req, res) {
  log("method: " + req.method);

  let body = getRequestBody(req);
  log("body: " + body);

  let contentType = req.hasHeader("Content-Type") ? req.getHeader("Content-Type") : null;
  log("contentType: " + contentType);

  let params = parseQuery(req.queryString);
  parseQuery(body, params);
  log("params: " + JSON.stringify(params));

  // Delegate an authentication request to the correct handler.
  if ("action" in params) {
    methodHandlers[params.action](req, res, params);
  } else {
    sendError(res, 501);
  }
}

function respondWithFile(res, fileName, mimeType) {
  res.setStatusLine("1.1", 200, "OK");
  res.setHeader("Content-Type", mimeType);

  let inputStream = getInputStream(kBasePath + fileName);
  res.bodyOutputStream.writeFrom(inputStream, inputStream.available());
  inputStream.close();
}

const methodHandlers = {
  auth: function(req, res, params) {
    respondWithFile(res, "google_auth.txt", "text/html");
  },

  token: function(req, res, params) {
    respondWithFile(res, "google_token.txt", "application/json");
  },

  contacts: function(req, res, params) {
    try {
      checkAuth(req);
    } catch (ex) {
      sendError(res, ex, ex.code);
      return;
    }

    respondWithFile(res, "google_contacts.txt", "text/xml");
  }
};
