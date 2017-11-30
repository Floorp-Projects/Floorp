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
   * 3. onDataAvailable decodes and inserts data into a text node
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
    // Decode and insert data.
    let buffer = new ArrayBuffer(count);
    new BinaryInput(inputStream).readArrayBuffer(count, buffer);
    this.decodeAndInsertBuffer(buffer);
  },

  onStartRequest: function (request, context) {
    // Set the content type to HTML in order to parse the doctype, styles
    // and scripts. The JSON will be manually inserted as text.
    request.QueryInterface(Ci.nsIChannel);
    request.contentType = "text/html";

    // Don't honor the charset parameter and use UTF-8 (see bug 741776).
    request.contentCharset = "UTF-8";
    this.decoder = new TextDecoder("UTF-8");

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
    insertJsonData(win, this.data.json);
    win.addEventListener("contentMessage", onContentMessage, false, true);
    keepThemeUpdated(win);

    // Send the initial HTML code.
    let buffer = new TextEncoder().encode(initialHTML(win.document)).buffer;
    let stream = new BufferStream(buffer, 0, buffer.byteLength);
    this.listener.onDataAvailable(request, context, stream, 0, stream.available());
  },

  onStopRequest: function (request, context, statusCode) {
    // Flush data.
    this.decodeAndInsertBuffer(new ArrayBuffer(0), true);

    // Stop the request.
    this.listener.onStopRequest(request, context, statusCode);
    this.listener = null;
    this.decoder = null;
    this.data = null;
  },

  // Decodes an ArrayBuffer into a string and inserts it into the page.
  decodeAndInsertBuffer: function (buffer, flush = false) {
    // Decode the buffer into a string.
    let data = this.decoder.decode(buffer, {stream: !flush});

    // Using `appendData` instead of `textContent +=` is important to avoid
    // repainting previous data.
    this.data.json.appendData(data);
  }
};

// Lets "save as" save the original JSON, not the viewer.
// To save with the proper extension we need the original content type,
// which has been replaced by application/vnd.mozilla.json.view
function fixSave(request) {
  let match;
  if (request instanceof Ci.nsIHttpChannel) {
    try {
      let header = request.getResponseHeader("Content-Type");
      match = header.match(/^(application\/(?:[^;]+\+)?json)(?:;|$)/);
    } catch (err) {
      // Handled below
    }
  } else {
    let uri = request.QueryInterface(Ci.nsIChannel).URI.spec;
    match = uri.match(/^data:(application\/(?:[^;,]+\+)?json)[;,]/);
  }
  let originalType;
  if (match) {
    originalType = match[1];
  } else {
    originalType = "application/json";
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

  data.json = new win.Text();

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

// Builds an HTML string that will be used to load stylesheets and scripts.
function initialHTML(doc) {
  // Creates an element with the specified type, attributes and children.
  function element(type, attributes = {}, children = []) {
    let el = doc.createElement(type);
    for (let [attr, value] of Object.entries(attributes)) {
      el.setAttribute(attr, value);
    }
    el.append(...children);
    return el;
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

  // The base URI is prepended to all URIs instead of using a <base> element
  // because the latter can be blocked by a CSP base-uri directive (bug 1316393)
  let baseURI = "resource://devtools-client-jsonview/";

  return "<!DOCTYPE html>\n" +
    element("html", {
      "platform": os,
      "class": "theme-" + Services.prefs.getCharPref("devtools.theme"),
      "dir": Services.locale.isAppLocaleRTL ? "rtl" : "ltr"
    }, [
      element("head", {}, [
        element("link", {
          rel: "stylesheet",
          type: "text/css",
          href: baseURI + "css/main.css",
        }),
        element("script", {
          src: baseURI + "lib/require.js",
          "data-main": baseURI + "viewer-config.js",
          defer: true,
        })
      ]),
      element("body", {}, [
        element("div", {"id": "content"}, [
          element("div", {"id": "json"})
        ])
      ])
    ]).outerHTML;
}

// We insert the received data into a text node, which should be appended into
// the #json element so that the JSON is still displayed even if JS is disabled.
// However, the HTML parser is not synchronous, so this function uses a mutation
// observer to detect the creation of the element. Then the text node is appended.
function insertJsonData(win, json) {
  new win.MutationObserver(function (mutations, observer) {
    for (let {target, addedNodes} of mutations) {
      if (target.nodeType == 1 && target.id == "content") {
        for (let node of addedNodes) {
          if (node.nodeType == 1 && node.id == "json") {
            observer.disconnect();
            node.append(json);
            return;
          }
        }
      }
    }
  }).observe(win.document, {
    childList: true,
    subtree: true,
  });
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
    case "save":
      childProcessMessageManager.sendAsyncMessage(
        "devtools:jsonview:save", value);
  }
}

function createInstance() {
  return new Converter();
}

exports.JsonViewService = {
  createInstance: createInstance,
};
