/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
});

const {
  getTheme,
  addThemeObserver,
  removeThemeObserver,
} = require("resource://devtools/client/shared/theme.js");

const BinaryInput = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);
const BufferStream = Components.Constructor(
  "@mozilla.org/io/arraybuffer-input-stream;1",
  "nsIArrayBufferInputStream",
  "setData"
);

const kCSP = "default-src 'none'; script-src resource:; img-src 'self';";

// Localization
loader.lazyGetter(this, "jsonViewStrings", () => {
  return Services.strings.createBundle(
    "chrome://devtools/locale/jsonview.properties"
  );
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
  QueryInterface: ChromeUtils.generateQI([
    "nsIStreamConverter",
    "nsIStreamListener",
    "nsIRequestObserver",
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
  convert(fromStream, fromType, toType, ctx) {
    return fromStream;
  },

  asyncConvertData(fromType, toType, listener, ctx) {
    this.listener = listener;
  },
  getConvertedType(fromType, channel) {
    return "text/html";
  },

  onDataAvailable(request, inputStream, offset, count) {
    // Decode and insert data.
    const buffer = new ArrayBuffer(count);
    new BinaryInput(inputStream).readArrayBuffer(count, buffer);
    this.decodeAndInsertBuffer(buffer);
  },

  onStartRequest(request) {
    // Set the content type to HTML in order to parse the doctype, styles
    // and scripts. The JSON will be manually inserted as text.
    request.QueryInterface(Ci.nsIChannel);
    request.contentType = "text/html";

    // Tweak the request's principal in order to allow the related HTML document
    // used to display raw JSON to be able to load resource://devtools files
    // from the jsonview document.
    const uri = lazy.NetUtil.newURI("resource://devtools/client/jsonview/");
    const resourcePrincipal =
      Services.scriptSecurityManager.createContentPrincipal(
        uri,
        request.loadInfo.originAttributes
      );
    request.owner = resourcePrincipal;

    const headers = getHttpHeaders(request);

    // Enforce strict CSP:
    try {
      request.QueryInterface(Ci.nsIHttpChannel);
      request.setResponseHeader("Content-Security-Policy", kCSP, false);
      request.setResponseHeader(
        "Content-Security-Policy-Report-Only",
        "",
        false
      );
    } catch (ex) {
      // If this is not an HTTP channel we can't and won't do anything.
    }

    // Don't honor the charset parameter and use UTF-8 (see bug 741776).
    request.contentCharset = "UTF-8";
    this.decoder = new TextDecoder("UTF-8");

    // Changing the content type breaks saving functionality. Fix it.
    fixSave(request);

    // Start the request.
    this.listener.onStartRequest(request);

    // Initialize stuff.
    const win = getWindowForRequest(request);
    if (!win || !Components.isSuccessCode(request.status)) {
      return;
    }

    // We compare actual pointer identities here rather than using .equals(),
    // because if things went correctly then the document must have exactly
    // the principal we reset it to above. If not, something went wrong.
    if (win.document.nodePrincipal != resourcePrincipal) {
      // Whatever that document is, it's not ours.
      request.cancel(Cr.NS_BINDING_ABORTED);
      return;
    }

    this.data = exportData(win, headers);
    insertJsonData(win, this.data.json);
    win.addEventListener("contentMessage", onContentMessage, false, true);
    keepThemeUpdated(win);

    // Send the initial HTML code.
    const buffer = new TextEncoder().encode(initialHTML(win.document)).buffer;
    const stream = new BufferStream(buffer, 0, buffer.byteLength);
    this.listener.onDataAvailable(request, stream, 0, stream.available());
  },

  onStopRequest(request, statusCode) {
    // Flush data if we haven't been canceled.
    if (Components.isSuccessCode(statusCode)) {
      this.decodeAndInsertBuffer(new ArrayBuffer(0), true);
    }

    // Stop the request.
    this.listener.onStopRequest(request, statusCode);
    this.listener = null;
    this.decoder = null;
    this.data = null;
  },

  // Decodes an ArrayBuffer into a string and inserts it into the page.
  decodeAndInsertBuffer(buffer, flush = false) {
    // Decode the buffer into a string.
    const data = this.decoder.decode(buffer, { stream: !flush });

    // Using `appendData` instead of `textContent +=` is important to avoid
    // repainting previous data.
    this.data.json.appendData(data);
  },
};

// Lets "save as" save the original JSON, not the viewer.
// To save with the proper extension we need the original content type,
// which has been replaced by application/vnd.mozilla.json.view
function fixSave(request) {
  let match;
  if (request instanceof Ci.nsIHttpChannel) {
    try {
      const header = request.getResponseHeader("Content-Type");
      match = header.match(/^(application\/(?:[^;]+\+)?json)(?:;|$)/);
    } catch (err) {
      // Handled below
    }
  } else {
    const uri = request.QueryInterface(Ci.nsIChannel).URI.spec;
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

function getHttpHeaders(request) {
  const headers = {
    response: [],
    request: [],
  };
  // The request doesn't have to be always nsIHttpChannel
  // (e.g. in case of data: URLs)
  if (request instanceof Ci.nsIHttpChannel) {
    request.visitResponseHeaders({
      visitHeader(name, value) {
        headers.response.push({ name, value });
      },
    });
    request.visitRequestHeaders({
      visitHeader(name, value) {
        headers.request.push({ name, value });
      },
    });
  }
  return headers;
}

let jsonViewStringDict = null;
function getAllStrings() {
  if (!jsonViewStringDict) {
    jsonViewStringDict = {};
    for (const string of jsonViewStrings.getSimpleEnumeration()) {
      jsonViewStringDict[string.key] = string.value;
    }
  }
  return jsonViewStringDict;
}

// The two following methods are duplicated from NetworkHelper.sys.mjs
// to avoid pulling the whole NetworkHelper as a dependency during
// initialization.

/**
 * Gets the nsIDOMWindow that is associated with request.
 *
 * @param nsIHttpChannel request
 * @returns nsIDOMWindow or null
 */
function getWindowForRequest(request) {
  try {
    return getRequestLoadContext(request).associatedWindow;
  } catch (ex) {
    // On some request notificationCallbacks and loadGroup are both null,
    // so that we can't retrieve any nsILoadContext interface.
    // Fallback on nsILoadInfo to try to retrieve the request's window.
    // (this is covered by test_network_get.html and its CSS request)
    return request.loadInfo.loadingDocument?.defaultView;
  }
}

/**
 * Gets the nsILoadContext that is associated with request.
 *
 * @param nsIHttpChannel request
 * @returns nsILoadContext or null
 */
function getRequestLoadContext(request) {
  try {
    return request.notificationCallbacks.getInterface(Ci.nsILoadContext);
  } catch (ex) {
    // Ignore.
  }

  try {
    return request.loadGroup.notificationCallbacks.getInterface(
      Ci.nsILoadContext
    );
  } catch (ex) {
    // Ignore.
  }

  return null;
}

// Exports variables that will be accessed by the non-privileged scripts.
function exportData(win, headers) {
  const json = new win.Text();
  const JSONView = Cu.cloneInto(
    {
      headers,
      json,
      readyState: "uninitialized",
      Locale: getAllStrings(),
    },
    win,
    {
      wrapReflectors: true,
    }
  );
  try {
    Object.defineProperty(Cu.waiveXrays(win), "JSONView", {
      value: JSONView,
      configurable: true,
      enumerable: true,
      writable: true,
    });
  } catch (error) {
    console.error(error);
  }
  return { json };
}

// Builds an HTML string that will be used to load stylesheets and scripts.
function initialHTML(doc) {
  // Creates an element with the specified type, attributes and children.
  function element(type, attributes = {}, children = []) {
    const el = doc.createElement(type);
    for (const [attr, value] of Object.entries(attributes)) {
      el.setAttribute(attr, value);
    }
    el.append(...children);
    return el;
  }

  let os;
  const platform = Services.appinfo.OS;
  if (platform.startsWith("WINNT")) {
    os = "win";
  } else if (platform.startsWith("Darwin")) {
    os = "mac";
  } else {
    os = "linux";
  }

  const baseURI = "resource://devtools/client/jsonview/";

  return (
    "<!DOCTYPE html>\n" +
    element(
      "html",
      {
        platform: os,
        class: "theme-" + getTheme(),
        dir: Services.locale.isAppLocaleRTL ? "rtl" : "ltr",
      },
      [
        element("head", {}, [
          element("meta", {
            "http-equiv": "Content-Security-Policy",
            content: kCSP,
          }),
          element("link", {
            rel: "stylesheet",
            type: "text/css",
            href: "chrome://devtools-jsonview-styles/content/main.css",
          }),
        ]),
        element("body", {}, [
          element("div", { id: "content" }, [element("div", { id: "json" })]),
          element("script", {
            src: baseURI + "lib/require.js",
            "data-main": baseURI + "viewer-config.js",
          }),
        ]),
      ]
    ).outerHTML
  );
}

// We insert the received data into a text node, which should be appended into
// the #json element so that the JSON is still displayed even if JS is disabled.
// However, the HTML parser is not synchronous, so this function uses a mutation
// observer to detect the creation of the element. Then the text node is appended.
function insertJsonData(win, json) {
  new win.MutationObserver(function (mutations, observer) {
    for (const { target, addedNodes } of mutations) {
      if (target.nodeType == 1 && target.id == "content") {
        for (const node of addedNodes) {
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
  const listener = function () {
    win.document.documentElement.className = "theme-" + getTheme();
  };
  addThemeObserver(listener);
  win.addEventListener(
    "unload",
    function (event) {
      removeThemeObserver(listener);
      win = null;
    },
    { once: true }
  );
}

// Chrome <-> Content communication
function onContentMessage(e) {
  // Do not handle events from different documents.
  const win = this;
  if (win != e.target) {
    return;
  }

  const value = e.detail.value;
  switch (e.detail.type) {
    case "save":
      win.docShell.messageManager.sendAsyncMessage(
        "devtools:jsonview:save",
        value
      );
  }
}

function createInstance() {
  return new Converter();
}

exports.JsonViewService = {
  createInstance,
};
