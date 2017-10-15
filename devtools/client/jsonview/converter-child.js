/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu, CC} = require("chrome");
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const Services = require("Services");

loader.lazyRequireGetter(this, "NetworkHelper",
                               "devtools/shared/webconsole/network-helper");
loader.lazyGetter(this, "debug", function () {
  let {AppConstants} = require("resource://gre/modules/AppConstants.jsm");
  return !!(AppConstants.DEBUG || AppConstants.DEBUG_JS_MODULES);
});

const childProcessMessageManager =
  Cc["@mozilla.org/childprocessmessagemanager;1"]
    .getService(Ci.nsISyncMessageSender);
const BinaryInput = CC("@mozilla.org/binaryinputstream;1",
                       "nsIBinaryInputStream", "setInputStream");
const BufferStream = CC("@mozilla.org/io/arraybuffer-input-stream;1",
                       "nsIArrayBufferInputStream", "setData");
const encodingLength = 2;
const encoder = new TextEncoder();

// Localization
loader.lazyGetter(this, "jsonViewStrings", () => {
  return Services.strings.createBundle(
    "chrome://devtools/locale/jsonview.properties");
});

/**
 * This object detects 'application/vnd.mozilla.json.view' content type
 * and converts it into a JSON Viewer application that allows simple
 * JSON inspection.
 *
 * Inspired by JSON View: https://github.com/bhollis/jsonview/
 */
function Converter() {}

Converter.prototype = {
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIStreamConverter,
    Ci.nsIStreamListener,
    Ci.nsIRequestObserver
  ]),

  get wrappedJSObject() {
    return this;
  },

  /**
   * This component works as such:
   * 1. asyncConvertData captures the listener
   * 2. onStartRequest fires, initializes stuff, modifies the listener
   *    to match our output type
   * 3. onDataAvailable converts to UTF-8 and spits back to the listener
   * 4. onStopRequest flushes data and spits back to the listener
   * 5. convert does nothing, it's just the synchronous version
   *    of asyncConvertData
   */
  convert: function (fromStream, fromType, toType, ctx) {
    return fromStream;
  },

  asyncConvertData: function (fromType, toType, listener, ctx) {
    this.listener = listener;
  },

  onDataAvailable: function (request, context, inputStream, offset, count) {
    // If the encoding is not known, store data in an array until we have enough bytes.
    if (this.encodingArray) {
      let desired = encodingLength - this.encodingArray.length;
      let n = Math.min(desired, count);
      let bytes = new BinaryInput(inputStream).readByteArray(n);
      offset += n;
      count -= n;
      this.encodingArray.push(...bytes);
      if (n < desired) {
        // Wait until there is more data.
        return;
      }
      this.determineEncoding(request, context);
    }

    // Spit back the data if the encoding is UTF-8, otherwise convert it first.
    if (!this.decoder) {
      this.listener.onDataAvailable(request, context, inputStream, offset, count);
    } else {
      let buffer = new ArrayBuffer(count);
      new BinaryInput(inputStream).readArrayBuffer(count, buffer);
      this.convertAndSendBuffer(request, context, buffer);
    }
  },

  onStartRequest: function (request, context) {
    // Set the content type to HTML in order to parse the doctype, styles
    // and scripts, but later a <plaintext> element will switch the tokenizer
    // to the plaintext state in order to parse the JSON.
    request.QueryInterface(Ci.nsIChannel);
    request.contentType = "text/html";

    // Don't honor the charset parameter and use UTF-8 (see bug 741776).
    request.contentCharset = "UTF-8";

    // Changing the content type breaks saving functionality. Fix it.
    fixSave(request);

    // Because content might still have a reference to this window,
    // force setting it to a null principal to avoid it being same-
    // origin with (other) content.
    request.loadInfo.resetPrincipalToInheritToNullPrincipal();

    // Start the request.
    this.listener.onStartRequest(request, context);

    // Initialize stuff.
    let win = NetworkHelper.getWindowForRequest(request);
    this.data = exportData(win, request);
    win.addEventListener("DOMContentLoaded", event => {
      win.addEventListener("contentMessage", onContentMessage, false, true);
    }, {once: true});
    keepThemeUpdated(win);

    // Send the initial HTML code.
    let bytes = encoder.encode(initialHTML(win.document));
    this.convertAndSendBuffer(request, context, bytes.buffer);

    // Create an array to store data until the encoding is determined.
    this.encodingArray = [];
  },

  onStopRequest: function (request, context, statusCode) {
    // Flush data.
    if (this.encodingArray) {
      this.determineEncoding(request, context, true);
    } else {
      this.convertAndSendBuffer(request, context, new ArrayBuffer(0), true);
    }

    // Stop the request.
    this.listener.onStopRequest(request, context, statusCode);
    this.listener = null;
    this.decoder = null;
    this.data = null;
  },

  // Determines the encoding of the response.
  determineEncoding: function (request, context, flush = false) {
    // Determine the encoding using the bytes in encodingArray, defaulting to UTF-8.
    // An initial byte order mark character (U+FEFF) does the trick.
    // If there is no BOM, since the first character of valid JSON will be ASCII,
    // the pattern of nulls in the first two bytes can be used instead.
    //  - UTF-16BE:  00 xx  or  FE FF
    //  - UTF-16LE:  xx 00  or  FF FE
    //  - UTF-8:  anything else.
    let encoding = "UTF-8";
    let bytes = this.encodingArray;
    if (bytes.length >= 2) {
      if (!bytes[0] && bytes[1] || bytes[0] == 0xFE && bytes[1] == 0xFF) {
        encoding = "UTF-16BE";
      } else if (bytes[0] && !bytes[1] || bytes[0] == 0xFF && bytes[1] == 0xFE) {
        encoding = "UTF-16LE";
      }
    }

    // Create a decoder unless the data is already in UTF-8.
    if (encoding !== "UTF-8") {
      this.decoder = new TextDecoder(encoding, {ignoreBOM: true});
    }

    this.data.encoding = encoding;

    // Send the bytes in encodingArray, and remove it.
    let buffer = new Uint8Array(bytes).buffer;
    this.convertAndSendBuffer(request, context, buffer, flush);
    this.encodingArray = null;
  },

  // Converts an ArrayBuffer to UTF-8 and sends it.
  convertAndSendBuffer: function (request, context, buffer, flush = false) {
    // If the encoding is not UTF-8, decode the buffer and encode into UTF-8.
    if (this.decoder) {
      let data = this.decoder.decode(buffer, {stream: !flush});
      buffer = encoder.encode(data).buffer;
    }

    // Create an input stream that contains the bytes in the buffer.
    let stream = new BufferStream(buffer, 0, buffer.byteLength);

    // Send the input stream.
    this.listener.onDataAvailable(request, context, stream, 0, stream.available());
  }
};

// Lets "save as" save the original JSON, not the viewer.
// To save with the proper extension we need the original content type,
// which has been replaced by application/vnd.mozilla.json.view
function fixSave(request) {
  let originalType;
  if (request instanceof Ci.nsIHttpChannel) {
    try {
      let header = request.getResponseHeader("Content-Type");
      originalType = header.split(";")[0];
    } catch (err) {
      // Handled below
    }
  } else {
    let uri = request.QueryInterface(Ci.nsIChannel).URI.spec;
    let match = uri.match(/^data:(.*?)[,;]/);
    if (match) {
      originalType = match[1];
    }
  }
  const JSON_TYPES = ["application/json", "application/manifest+json"];
  if (!JSON_TYPES.includes(originalType)) {
    originalType = JSON_TYPES[0];
  }
  request.QueryInterface(Ci.nsIWritablePropertyBag);
  request.setProperty("contentType", originalType);
}

// Exports variables that will be accessed by the non-privileged scripts.
function exportData(win, request) {
  let data = Cu.createObjectIn(win, {
    defineAs: "JSONView"
  });

  data.debug = debug;

  let Locale = {
    $STR: key => {
      try {
        return jsonViewStrings.GetStringFromName(key);
      } catch (err) {
        console.error(err);
        return undefined;
      }
    }
  };
  data.Locale = Cu.cloneInto(Locale, win, {cloneFunctions: true});

  let headers = {
    response: [],
    request: []
  };
  // The request doesn't have to be always nsIHttpChannel
  // (e.g. in case of data: URLs)
  if (request instanceof Ci.nsIHttpChannel) {
    request.visitResponseHeaders({
      visitHeader: function (name, value) {
        headers.response.push({name: name, value: value});
      }
    });
    request.visitRequestHeaders({
      visitHeader: function (name, value) {
        headers.request.push({name: name, value: value});
      }
    });
  }
  data.headers = Cu.cloneInto(headers, win);

  return data;
}

// Serializes a qualifiedName and an optional set of attributes into an HTML
// start tag. Be aware qualifiedName and attribute names are not validated.
// Attribute values are escaped with escapingString algorithm in attribute mode
// (https://html.spec.whatwg.org/multipage/syntax.html#escapingString).
function startTag(qualifiedName, attributes = {}) {
  return Object.entries(attributes).reduce(function (prev, [attr, value]) {
    return prev + " " + attr + "=\"" +
      value.replace(/&/g, "&amp;")
           .replace(/\u00a0/g, "&nbsp;")
           .replace(/"/g, "&quot;") +
      "\"";
  }, "<" + qualifiedName) + ">";
}

// Builds an HTML string that will be used to load stylesheets and scripts,
// and switch the parser to plaintext state.
function initialHTML(doc) {
  let os;
  let platform = Services.appinfo.OS;
  if (platform.startsWith("WINNT")) {
    os = "win";
  } else if (platform.startsWith("Darwin")) {
    os = "mac";
  } else {
    os = "linux";
  }

  // The base URI is prepended to all URIs instead of using a <base> element
  // because the latter can be blocked by a CSP base-uri directive (bug 1316393)
  let baseURI = "resource://devtools-client-jsonview/";

  let style = doc.createElement("link");
  style.rel = "stylesheet";
  style.type = "text/css";
  style.href = baseURI + "css/main.css";

  let script = doc.createElement("script");
  script.src = baseURI + "lib/require.js";
  script.dataset.main = baseURI + "viewer-config.js";
  script.defer = true;

  let head = doc.createElement("head");
  head.append(style, script);

  return "<!DOCTYPE html>\n" +
    startTag("html", {
      "platform": os,
      "class": "theme-" + Services.prefs.getCharPref("devtools.theme"),
      "dir": Services.locale.isAppLocaleRTL ? "rtl" : "ltr"
    }) +
    head.outerHTML +
    startTag("body") +
    startTag("div", {"id": "content"}) +
    startTag("plaintext", {"id": "json"});
}

function keepThemeUpdated(win) {
  let listener = function () {
    let theme = Services.prefs.getCharPref("devtools.theme");
    win.document.documentElement.className = "theme-" + theme;
  };
  Services.prefs.addObserver("devtools.theme", listener);
  win.addEventListener("unload", function (event) {
    Services.prefs.removeObserver("devtools.theme", listener);
    win = null;
  }, {once: true});
}

// Chrome <-> Content communication
function onContentMessage(e) {
  // Do not handle events from different documents.
  let win = this;
  if (win != e.target) {
    return;
  }

  let value = e.detail.value;
  switch (e.detail.type) {
    case "copy":
      copyString(win, value);
      break;

    case "copy-headers":
      copyHeaders(win, value);
      break;

    case "save":
      childProcessMessageManager.sendAsyncMessage(
        "devtools:jsonview:save", value);
  }
}

function copyHeaders(win, headers) {
  let value = "";
  let eol = (Services.appinfo.OS !== "WINNT") ? "\n" : "\r\n";

  let responseHeaders = headers.response;
  for (let i = 0; i < responseHeaders.length; i++) {
    let header = responseHeaders[i];
    value += header.name + ": " + header.value + eol;
  }

  value += eol;

  let requestHeaders = headers.request;
  for (let i = 0; i < requestHeaders.length; i++) {
    let header = requestHeaders[i];
    value += header.name + ": " + header.value + eol;
  }

  copyString(win, value);
}

function copyString(win, string) {
  win.document.addEventListener("copy", event => {
    event.clipboardData.setData("text/plain", string);
    event.preventDefault();
  }, {once: true});

  win.document.execCommand("copy", false, null);
}

function createInstance() {
  return new Converter();
}

exports.JsonViewService = {
  createInstance: createInstance,
};
