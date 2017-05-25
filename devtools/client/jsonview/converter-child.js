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
   * 3. onDataAvailable spits it back to the listener
   * 4. onStopRequest spits it back to the listener and initializes
        the JSON Viewer
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
    this.listener.onDataAvailable(...arguments);
  },

  onStartRequest: function (request, context) {
    this.channel = request;

    // Let "save as" save the original JSON, not the viewer.
    // To save with the proper extension we need the original content type,
    // which has been replaced by application/vnd.mozilla.json.view
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

    // Parse source as JSON. This is like text/plain, but enforcing
    // UTF-8 charset (see bug 741776).
    request.QueryInterface(Ci.nsIChannel);
    request.contentType = JSON_TYPES[0];
    this.charset = request.contentCharset = "UTF-8";

    // Because content might still have a reference to this window,
    // force setting it to a null principal to avoid it being same-
    // origin with (other) content.
    request.loadInfo.resetPrincipalToInheritToNullPrincipal();

    this.listener.onStartRequest(request, context);
  },

  onStopRequest: function (request, context, statusCode) {
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

    let win = NetworkHelper.getWindowForRequest(request);
    JsonViewUtils.exportIntoContentScope(win, Locale, "Locale");
    JsonViewUtils.exportIntoContentScope(win, headers, "headers");

    win.addEventListener("DOMContentLoaded", event => {
      win.addEventListener("contentMessage",
        onContentMessage.bind(this), false, true);
      loadJsonViewer(win.document);
    }, {once: true});

    this.listener.onStopRequest(this.channel, context, statusCode);
    this.listener = null;
  }
};

// Chrome <-> Content communication
function onContentMessage(e) {
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
      copyHeaders(win, value);
      break;

    case "save":
      // The window ID is needed when the JSON Viewer is inside an iframe.
      let windowID = win.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
      childProcessMessageManager.sendAsyncMessage(
        "devtools:jsonview:save", {url: value, windowID: windowID});
  }
}

// Loads the JSON Viewer into a text/plain document
function loadJsonViewer(doc) {
  function addStyleSheet(url) {
    let link = doc.createElement("link");
    link.rel = "stylesheet";
    link.type = "text/css";
    link.href = url;
    doc.head.appendChild(link);
  }

  let os;
  let platform = Services.appinfo.OS;
  if (platform.startsWith("WINNT")) {
    os = "win";
  } else if (platform.startsWith("Darwin")) {
    os = "mac";
  } else {
    os = "linux";
  }

  doc.documentElement.setAttribute("platform", os);
  doc.documentElement.dataset.contentType = doc.contentType;
  doc.documentElement.classList.add("theme-" + JsonViewUtils.getCurrentTheme());
  doc.documentElement.dir = Services.locale.isAppLocaleRTL ? "rtl" : "ltr";

  let base = doc.createElement("base");
  base.href = "resource://devtools/client/jsonview/";
  doc.head.appendChild(base);

  addStyleSheet("../themes/variables.css");
  addStyleSheet("../themes/common.css");
  addStyleSheet("../themes/toolbars.css");
  addStyleSheet("css/main.css");

  let json = doc.querySelector("pre");
  json.id = "json";
  let content = doc.createElement("div");
  content.id = "content";
  content.appendChild(json);
  doc.body.appendChild(content);

  let script = doc.createElement("script");
  script.src = "lib/require.js";
  script.dataset.main = "viewer-config";
  doc.body.appendChild(script);
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
