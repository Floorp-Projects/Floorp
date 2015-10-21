/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Cc, Ci, components} = require("chrome");
const {Class} = require("sdk/core/heritage");
const {Unknown} = require("sdk/platform/xpcom");
const xpcom = require("sdk/platform/xpcom");
const Events = require("sdk/dom/events");
const Clipboard = require("sdk/clipboard");

loader.lazyRequireGetter(this, "NetworkHelper",
                               "devtools/shared/webconsole/network-helper");
loader.lazyRequireGetter(this, "JsonViewUtils",
                               "devtools/client/jsonview/utils");

const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

const childProcessMessageManager =
  Cc["@mozilla.org/childprocessmessagemanager;1"].
    getService(Ci.nsISyncMessageSender);

// Amount of space that will be allocated for the stream's backing-store.
// Must be power of 2. Used to copy the data stream in onStopRequest.
const SEGMENT_SIZE = Math.pow(2, 17);

const JSON_VIEW_MIME_TYPE = "application/vnd.mozilla.json.view";
const CONTRACT_ID = "@mozilla.org/streamconv;1?from=" + JSON_VIEW_MIME_TYPE + "&to=*/*";
const CLASS_ID = "{d8c9acee-dec5-11e4-8c75-1681e6b88ec1}";

// Localization
var jsonViewStrings = Services.strings.createBundle(
  "chrome://browser/locale/devtools/jsonview.properties");

/**
 * This object detects 'application/vnd.mozilla.json.view' content type
 * and converts it into a JSON Viewer application that allows simple
 * JSON inspection.
 *
 * Inspired by JSON View: https://github.com/bhollis/jsonview/
 */
var Converter = Class({
  extends: Unknown,

  interfaces: [
    "nsIStreamConverter",
    "nsIStreamListener",
    "nsIRequestObserver"
  ],

  get wrappedJSObject() this,

  /**
   * This component works as such:
   * 1. asyncConvertData captures the listener
   * 2. onStartRequest fires, initializes stuff, modifies the listener to match our output type
   * 3. onDataAvailable transcodes the data into a UTF-8 string
   * 4. onStopRequest gets the collected data and converts it, spits it to the listener
   * 5. convert does nothing, it's just the synchronous version of asyncConvertData
   */
  convert: function(aFromStream, aFromType, aToType, aCtxt) {
    return aFromStream;
  },

  asyncConvertData: function(aFromType, aToType, aListener, aCtxt) {
    this.listener = aListener;
  },

  onDataAvailable: function(aRequest, aContext, aInputStream, aOffset, aCount) {
    // From https://developer.mozilla.org/en/Reading_textual_data
    var is = Cc["@mozilla.org/intl/converter-input-stream;1"].
      createInstance(Ci.nsIConverterInputStream);
    is.init(aInputStream, this.charset, -1,
      Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);

    // Seed it with something positive
    var bytesRead = 1;
    while (aCount) {
      var str = {};
      var bytesRead = is.readString(aCount, str);
      if (!bytesRead) {
        throw new Error("Stream converter failed to read the input stream!");
      }
      aCount -= bytesRead;
      this.data += str.value;
    }
  },

  onStartRequest: function(aRequest, aContext) {
    this.data = "";
    this.uri = aRequest.QueryInterface(Ci.nsIChannel).URI.spec;

    // Sets the charset if it is available. (For documents loaded from the
    // filesystem, this is not set.)
    this.charset = aRequest.QueryInterface(Ci.nsIChannel).contentCharset || 'UTF-8';

    this.channel = aRequest;
    this.channel.contentType = "text/html";
    this.channel.contentCharset = "UTF-8";

    this.listener.onStartRequest(this.channel, aContext);
  },

  /**
   * This should go something like this:
   * 1. Make sure we have a unicode string.
   * 2. Convert it to a Javascript object.
   * 2.1 Removes the callback
   * 3. Convert that to HTML? Or XUL?
   * 4. Spit it back out at the listener
   */
  onStopRequest: function(aRequest, aContext, aStatusCode) {
    let headers = {
      response: [],
      request: []
    }

    let win = NetworkHelper.getWindowForRequest(aRequest);

    let Locale = {
      $STR: key => {
        try {
          return jsonViewStrings.GetStringFromName(key);
        } catch (err) {
          Cu.reportError(err);
        }
      }
    };

    JsonViewUtils.exportIntoContentScope(win, Locale, "Locale");

    Events.once(win, "DOMContentLoaded", event => {
      Cu.exportFunction(this.postChromeMessage.bind(this), win, {
        defineAs: "postChromeMessage"
      });
    })

    // The request doesn't have to be always nsIHttpChannel
    // (e.g. in case of data: URLs)
    if (aRequest instanceof Ci.nsIHttpChannel) {
      aRequest.visitResponseHeaders({
        visitHeader: function(name, value) {
          headers.response.push({name: name, value: value});
        }
      });

      aRequest.visitRequestHeaders({
        visitHeader: function(name, value) {
          headers.request.push({name: name, value: value});
        }
      });
    }

    let outputDoc = "";

    try {
      headers = JSON.stringify(headers);
      outputDoc = this.toHTML(this.data, headers, this.uri);
    } catch (e) {
      Cu.reportError("JSON Viewer ERROR " + e);
      outputDoc = this.toErrorPage(e, this.data, this.uri);
    }

    var storage = Cc["@mozilla.org/storagestream;1"].createInstance(Ci.nsIStorageStream);
    storage.init(SEGMENT_SIZE, 0xffffffff, null);
    var out = storage.getOutputStream(0);

    var binout = Cc["@mozilla.org/binaryoutputstream;1"]
      .createInstance(Ci.nsIBinaryOutputStream);

    binout.setOutputStream(out);
    binout.writeUtf8Z(outputDoc);
    binout.close();

    // We need to trim 4 bytes off the front (this could be underlying bug).
    var trunc = 4;
    var instream = storage.newInputStream(trunc);

    // Pass the data to the main content listener
    this.listener.onDataAvailable(this.channel, aContext, instream, 0,
      instream.available());

    this.listener.onStopRequest(this.channel, aContext, aStatusCode);

    this.listener = null;
  },

  htmlEncode: function(t) {
    return t !== null ? t.toString().replace(/&/g,"&amp;").
      replace(/"/g,"&quot;").replace(/</g,"&lt;").replace(/>/g,"&gt;") : '';
  },

  toHTML: function(json, headers, title) {
    var themeClassName = "theme-" + JsonViewUtils.getCurrentTheme();
    var clientBaseUrl = "resource://devtools/client/";
    var baseUrl = clientBaseUrl + "jsonview/";
    var themeVarsUrl = clientBaseUrl + "themes/variables.css";

    return '<!DOCTYPE html>\n' +
      '<html class="' + themeClassName + '">' +
      '<head><title>' + this.htmlEncode(title) + '</title>' +
      '<base href="' + this.htmlEncode(baseUrl) + '">' +
      '<link rel="stylesheet" type="text/css" href="' + themeVarsUrl + '">' +
      '<link rel="stylesheet" type="text/css" href="css/main.css">' +
      '<script data-main="viewer-config" src="lib/require.js"></script>' +
      '</head><body>' +
      '<div id="content"></div>' +
      '<div id="json">' + this.htmlEncode(json) + '</div>' +
      '<div id="headers">' + this.htmlEncode(headers) + '</div>' +
      '</body></html>';
  },

  toErrorPage: function(error, data, uri) {
    // Escape unicode nulls
    data = data.replace("\u0000", "\uFFFD");

    var errorInfo = error + "";

    var output = '<div id="error">' + _('errorParsing')
    if (errorInfo.message) {
      output += '<div class="errormessage">' + errorInfo.message + '</div>';
    }

    output += '</div><div id="json">' + this.highlightError(data,
      errorInfo.line, errorInfo.column) + '</div>';

    return '<!DOCTYPE html>\n' +
      '<html><head><title>' + this.htmlEncode(uri + ' - Error') + '</title>' +
      '<base href="' + this.htmlEncode(self.data.url()) + '">' +
      '</head><body>' +
      output +
      '</body></html>';
  },

  // Chrome <-> Content communication

  postChromeMessage: function(type, args, objects) {
    var value = args;

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

  copyHeaders: function(headers) {
    var value = "";
    var eol = (Services.appinfo.OS !== "WINNT") ? "\n" : "\r\n";

    var responseHeaders = headers.response;
    for (var i=0; i<responseHeaders.length; i++) {
      var header = responseHeaders[i];
      value += header.name + ": " + header.value + eol;
    }

    value += eol;

    var requestHeaders = headers.request;
    for (var i=0; i<requestHeaders.length; i++) {
      var header = requestHeaders[i];
      value += header.name + ": " + header.value + eol;
    }

    Clipboard.set(value, "text");
  }
});

// Stream converter component definition
var service = xpcom.Service({
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
}
