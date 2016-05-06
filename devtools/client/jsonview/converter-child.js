/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Cc, Ci, components} = require("chrome");
const Services = require("Services");
const {Class} = require("sdk/core/heritage");
const {Unknown} = require("sdk/platform/xpcom");
const xpcom = require("sdk/platform/xpcom");
const Events = require("sdk/dom/events");
const Clipboard = require("sdk/clipboard");

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

const JSON_VIEW_MIME_TYPE = "application/vnd.mozilla.json.view";
const CONTRACT_ID = "@mozilla.org/streamconv;1?from=" +
  JSON_VIEW_MIME_TYPE + "&to=*/*";
const CLASS_ID = "{d8c9acee-dec5-11e4-8c75-1681e6b88ec1}";

// Localization
let jsonViewStrings = Services.strings.createBundle(
  "chrome://devtools/locale/jsonview.properties");

/**
 * This object detects 'application/vnd.mozilla.json.view' content type
 * and converts it into a JSON Viewer application that allows simple
 * JSON inspection.
 *
 * Inspired by JSON View: https://github.com/bhollis/jsonview/
 */
let Converter = Class({
  extends: Unknown,

  interfaces: [
    "nsIStreamConverter",
    "nsIStreamListener",
    "nsIRequestObserver"
  ],

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

    this.channel = request;
    this.channel.contentType = "text/html";
    this.channel.contentCharset = "UTF-8";

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

    Events.once(win, "DOMContentLoaded", event => {
      Cu.exportFunction(this.postChromeMessage.bind(this), win, {
        defineAs: "postChromeMessage"
      });
    });

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
      outputDoc = this.toHTML(this.data, headers, this.uri);
    } catch (e) {
      console.error("JSON Viewer ERROR " + e);
      outputDoc = this.toErrorPage(e, this.data, this.uri);
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

  toHTML: function (json, headers, title) {
    let themeClassName = "theme-" + JsonViewUtils.getCurrentTheme();
    let clientBaseUrl = "resource://devtools/client/";
    let baseUrl = clientBaseUrl + "jsonview/";
    let themeVarsUrl = clientBaseUrl + "themes/variables.css";
    let commonUrl = clientBaseUrl + "themes/common.css";

    return "<!DOCTYPE html>\n" +
      "<html class=\"" + themeClassName + "\">" +
      "<head><title>" + this.htmlEncode(title) + "</title>" +
      "<base href=\"" + this.htmlEncode(baseUrl) + "\">" +
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"" +
        themeVarsUrl + "\">" +
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"" +
        commonUrl + "\">" +
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"css/main.css\">" +
      "<script data-main=\"viewer-config\" src=\"lib/require.js\"></script>" +
      "</head><body>" +
      "<div id=\"content\"></div>" +
      "<div id=\"json\">" + this.htmlEncode(json) + "</div>" +
      "<div id=\"headers\">" + this.htmlEncode(headers) + "</div>" +
      "</body></html>";
  },

  toErrorPage: function (error, data, uri) {
    // Escape unicode nulls
    data = data.replace("\u0000", "\uFFFD");

    let errorInfo = error + "";

    let output = "<div id=\"error\">" + "error parsing";
    if (errorInfo.message) {
      output += "<div class=\"errormessage\">" + errorInfo.message + "</div>";
    }

    output += "</div><div id=\"json\">" + this.highlightError(data,
      errorInfo.line, errorInfo.column) + "</div>";

    return "<!DOCTYPE html>\n" +
      "<html><head><title>" + this.htmlEncode(uri + " - Error") + "</title>" +
      "<base href=\"" + this.htmlEncode(this.data.url()) + "\">" +
      "</head><body>" +
      output +
      "</body></html>";
  },

  // Chrome <-> Content communication

  postChromeMessage: function (type, args, objects) {
    let value = args;

    switch (type) {
      case "copy":
        Clipboard.set(value, "text");
        break;

      case "copy-headers":
        this.copyHeaders(value);
        break;

      case "save":
        childProcessMessageManager.sendAsyncMessage(
          "devtools:jsonview:save", value);
    }
  },

  copyHeaders: function (headers) {
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

    Clipboard.set(value, "text");
  }
});

// Stream converter component definition
let service = xpcom.Service({
  id: components.ID(CLASS_ID),
  contract: CONTRACT_ID,
  Component: Converter,
  register: false,
  unregister: false
});

function register() {
  if (!xpcom.isRegistered(service)) {
    xpcom.register(service);
    return true;
  }
  return false;
}

function unregister() {
  if (xpcom.isRegistered(service)) {
    xpcom.unregister(service);
    return true;
  }
  return false;
}

exports.JsonViewService = {
  register: register,
  unregister: unregister
};
