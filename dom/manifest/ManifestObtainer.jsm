/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
 * ManifestObtainer is an implementation of:
 * http://w3c.github.io/manifest/#obtaining
 *
 * Exposes 2 public method:
 *
 *  .contentObtainManifest(aContent) - used in content process
 *  .browserObtainManifest(aBrowser) - used in browser/parent process
 *
 * both return a promise. If successful, you get back a manifest object.
 *
 * Import it with URL:
 *   'chrome://global/content/manifestMessages.js'
 *
 * e10s IPC message from this components are handled by:
 *   dom/ipc/manifestMessages.js
 *
 * Which is injected into every browser instance via browser.js.
 */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { ManifestProcessor } = ChromeUtils.import(
  "resource://gre/modules/ManifestProcessor.jsm"
);

var ManifestObtainer = {
  /**
   * Public interface for obtaining a web manifest from a XUL browser, to use
   * on the parent process.
   * @param  {XULBrowser} The browser to check for the manifest.
   * @param {Object} aOptions
   * @param {Boolean} aOptions.checkConformance If spec conformance messages should be collected.
   *                                            Adds proprietary moz_* members to manifest.
   * @return {Promise<Object>} The processed manifest.
   */
  async browserObtainManifest(
    aBrowser,
    aOptions = { checkConformance: false }
  ) {
    if (!isXULBrowser(aBrowser)) {
      throw new TypeError("Invalid input. Expected XUL browser.");
    }

    const actor = aBrowser.browsingContext.currentWindowGlobal.getActor(
      "ManifestMessages"
    );

    const reply = await actor.sendQuery(
      "DOM:ManifestObtainer:Obtain",
      aOptions
    );
    if (!reply.success) {
      const error = toError(reply.result);
      throw error;
    }
    return reply.result;
  },
  /**
   * Public interface for obtaining a web manifest from a XUL browser.
   * @param {Window} aContent A content Window from which to extract the manifest.
   * @param {Object} aOptions
   * @param {Boolean} aOptions.checkConformance If spec conformance messages should be collected.
   *                                            Adds proprietary moz_* members to manifest.
   * @return {Promise<Object>} The processed manifest.
   */
  async contentObtainManifest(
    aContent,
    aOptions = { checkConformance: false }
  ) {
    if (!Services.prefs.getBoolPref("dom.manifest.enabled")) {
      throw new Error(
        "Obtaining manifest is disabled by pref: dom.manifest.enabled"
      );
    }
    if (!aContent || isXULBrowser(aContent)) {
      const err = new TypeError("Invalid input. Expected a DOM Window.");
      return Promise.reject(err);
    }
    const response = await fetchManifest(aContent);
    const result = await processResponse(response, aContent, aOptions);
    const clone = Cu.cloneInto(result, aContent);
    return clone;
  },
};

function toError(aErrorClone) {
  let error;
  switch (aErrorClone.name) {
    case "TypeError":
      error = new TypeError();
      break;
    default:
      error = new Error();
  }
  Object.getOwnPropertyNames(aErrorClone).forEach(
    name => (error[name] = aErrorClone[name])
  );
  return error;
}

function isXULBrowser(aBrowser) {
  if (!aBrowser || !aBrowser.namespaceURI || !aBrowser.localName) {
    return false;
  }
  const XUL_NS =
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  return aBrowser.namespaceURI === XUL_NS && aBrowser.localName === "browser";
}

/**
 * Asynchronously processes the result of response after having fetched
 * a manifest.
 * @param {Response} aResp Response from fetch().
 * @param {Window} aContentWindow The content window.
 * @return {Promise<Object>} The processed manifest.
 */
async function processResponse(aResp, aContentWindow, aOptions) {
  const badStatus = aResp.status < 200 || aResp.status >= 300;
  if (aResp.type === "error" || badStatus) {
    const msg = `Fetch error: ${aResp.status} - ${aResp.statusText} at ${aResp.url}`;
    throw new Error(msg);
  }
  const text = await aResp.text();
  const args = {
    jsonText: text,
    manifestURL: aResp.url,
    docURL: aContentWindow.location.href,
  };
  const processingOptions = Object.assign({}, args, aOptions);
  const manifest = ManifestProcessor.process(processingOptions);
  return manifest;
}

/**
 * Asynchronously fetches a web manifest.
 * @param {Window} a The content Window from where to extract the manifest.
 * @return {Promise<Object>}
 */
async function fetchManifest(aWindow) {
  if (!aWindow || aWindow.top !== aWindow) {
    const msg = "Window must be a top-level browsing context.";
    throw new Error(msg);
  }
  const elem = aWindow.document.querySelector("link[rel~='manifest']");
  if (!elem || !elem.getAttribute("href")) {
    // There is no actual manifest to fetch, we just return null.
    return new aWindow.Response("null");
  }
  // Throws on malformed URLs
  const manifestURL = new aWindow.URL(elem.href, elem.baseURI);
  const reqInit = {
    credentials: "omit",
    mode: "cors",
  };
  if (elem.crossOrigin === "use-credentials") {
    reqInit.credentials = "include";
  }
  const request = new aWindow.Request(manifestURL, reqInit);
  request.overrideContentPolicyType(Ci.nsIContentPolicy.TYPE_WEB_MANIFEST);
  // Can reject...
  return aWindow.fetch(request);
}

var EXPORTED_SYMBOLS = ["ManifestObtainer"];
