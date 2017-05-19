/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const Services = require("Services");

loader.lazyRequireGetter(this, "NetworkHelper",
                               "devtools/shared/webconsole/network-helper");
loader.lazyRequireGetter(this, "JsonViewUtils",
                               "devtools/client/jsonview/utils");

const childProcessMessageManager =
  Cc["@mozilla.org/childprocessmessagemanager;1"]
    .getService(Ci.nsISyncMessageSender);

// Amount of space that will be allocated for the stream's backing-store.
// Must be power of 2. Used to copy the data stream in onStopRequest.
const SEGMENT_SIZE = Math.pow(2, 17);

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
   * 3. onDataAvailable transcodes the data into a UTF-8 string
   * 4. onStopRequest gets the collected data and converts it,
   *    spits it to the listener
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
    // From https://developer.mozilla.org/en/Reading_textual_data
    let is = Cc["@mozilla.org/intl/converter-input-stream;1"]
      .createInstance(Ci.nsIConverterInputStream);
    is.init(inputStream, this.charset, -1,
      Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);

    // Seed it with something positive
    while (count) {
      let str = {};
      let bytesRead = is.readString(count, str);
      if (!bytesRead) {
        break;
      }
      count -= bytesRead;
      this.data += str.value;
    }
  },

  onStartRequest: function (request, context) {
    this.data = "";
    this.uri = request.QueryInterface(Ci.nsIChannel).URI.spec;

    // Sets the charset if it is available. (For documents loaded from the
    // filesystem, this is not set.)
    this.charset =
      request.QueryInterface(Ci.nsIChannel).contentCharset || "UTF-8";

    // Let "save as" save the original JSON, not the viewer.
    // To save with the proper extension we need the original content type,
    // which has been replaced by application/vnd.mozilla.json.view
    let originalType;
    if (request instanceof Ci.nsIHttpChannel) {
      try {
        originalType = request.getResponseHeader("Content-Type");
      } catch (err) {
        // Handled below
      }
    } else {
      let match = this.uri.match(/^data:(.*?)[,;]/);
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

    this.channel = request;
    this.channel.contentType = "text/html";
    this.channel.contentCharset = "UTF-8";
    // Because content might still have a reference to this window,
    // force setting it to a null principal to avoid it being same-
    // origin with (other) content.
    this.channel.loadInfo.resetPrincipalToInheritToNullPrincipal();

    this.listener.onStartRequest(this.channel, context);
  },

  /**
   * This should go something like this:
   * 1. Make sure we have a unicode string.
   * 2. Convert it to a Javascript object.
   * 2.1 Removes the callback
   * 3. Convert that to HTML? Or XUL?
   * 4. Spit it back out at the listener
   */
  onStopRequest: function (request, context, statusCode) {
    let headers = {
      response: [],
      request: []
    };

    let win = NetworkHelper.getWindowForRequest(request);

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

    JsonViewUtils.exportIntoContentScope(win, Locale, "Locale");

    win.addEventListener("DOMContentLoaded", event => {
      win.addEventListener("contentMessage",
        this.onContentMessage.bind(this), false, true);
    }, {once: true});

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

    let outputDoc = "";

    try {
      headers = JSON.stringify(headers);
      outputDoc = this.toHTML(this.data, headers);
    } catch (e) {
      console.error("JSON Viewer ERROR " + e);
      outputDoc = this.toErrorPage(e, this.data);
    }

    let storage = Cc["@mozilla.org/storagestream;1"]
      .createInstance(Ci.nsIStorageStream);

    storage.init(SEGMENT_SIZE, 0xffffffff, null);
    let out = storage.getOutputStream(0);

    let binout = Cc["@mozilla.org/binaryoutputstream;1"]
      .createInstance(Ci.nsIBinaryOutputStream);

    binout.setOutputStream(out);
    binout.writeUtf8Z(outputDoc);
    binout.close();

    // We need to trim 4 bytes off the front (this could be underlying bug).
    let trunc = 4;
    let instream = storage.newInputStream(trunc);

    // Pass the data to the main content listener
    this.listener.onDataAvailable(this.channel, context, instream, 0,
      instream.available());

    this.listener.onStopRequest(this.channel, context, statusCode);

    this.listener = null;
  },

  htmlEncode: function (t) {
    return t !== null ? t.toString()
      .replace(/&/g, "&amp;")
      .replace(/"/g, "&quot;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;") : "";
  },

  toHTML: function (json, headers) {
    let themeClassName = "theme-" + JsonViewUtils.getCurrentTheme();
    let clientBaseUrl = "resource://devtools/client/";
    let baseUrl = clientBaseUrl + "jsonview/";
    let themeVarsUrl = clientBaseUrl + "themes/variables.css";
    let commonUrl = clientBaseUrl + "themes/common.css";
    let toolbarsUrl = clientBaseUrl + "themes/toolbars.css";

    let os;
    let platform = Services.appinfo.OS;
    if (platform.startsWith("WINNT")) {
      os = "win";
    } else if (platform.startsWith("Darwin")) {
      os = "mac";
    } else {
      os = "linux";
    }

    let dir = Services.locale.isAppLocaleRTL ? "rtl" : "ltr";

    return "<!DOCTYPE html>\n" +
      "<html platform=\"" + os + "\" class=\"" + themeClassName +
        "\" dir=\"" + dir + "\">" +
      "<head>" +
      "<base href=\"" + this.htmlEncode(baseUrl) + "\">" +
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"" +
        themeVarsUrl + "\">" +
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"" +
        commonUrl + "\">" +
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"" +
        toolbarsUrl + "\">" +
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"css/main.css\">" +
      "<script data-main=\"viewer-config\" src=\"lib/require.js\"></script>" +
      "</head><body>" +
      "<div id=\"content\">" +
      "<pre id=\"json\">" + this.htmlEncode(json) + "</pre>" +
      "</div><div id=\"headers\">" + this.htmlEncode(headers) + "</div>" +
      "</body></html>";
  },

  toErrorPage: function (error, data) {
    // Escape unicode nulls
    data = data.replace("\u0000", "\uFFFD");

    let errorInfo = error + "";

    let output = "<div id=\"error\">" + "error parsing";
    if (errorInfo.message) {
      output += "<div class=\"errormessage\">" + errorInfo.message + "</div>";
    }

    output += "</div><div id=\"json\">" + this.highlightError(data,
      errorInfo.line, errorInfo.column) + "</div>";

    let dir = Services.locale.isAppLocaleRTL ? "rtl" : "ltr";

    return "<!DOCTYPE html>\n" +
      "<html><head>" +
      "<base href=\"" + this.htmlEncode(this.data.url()) + "\">" +
      "</head><body dir=\"" + dir + "\">" +
      output +
      "</body></html>";
  },

  // Chrome <-> Content communication

  onContentMessage: function (e) {
    // Do not handle events from different documents.
    let win = NetworkHelper.getWindowForRequest(this.channel);
    if (win != e.target) {
      return;
    }

    let value = e.detail.value;
    switch (e.detail.type) {
      case "copy":
        copyString(win, value);
        break;

      case "copy-headers":
        this.copyHeaders(win, value);
        break;

      case "save":
        // The window ID is needed when the JSON Viewer is inside an iframe.
        let windowID = win.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
        childProcessMessageManager.sendAsyncMessage(
          "devtools:jsonview:save", {url: value, windowID: windowID});
    }
  },

  copyHeaders: function (win, headers) {
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
};

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
