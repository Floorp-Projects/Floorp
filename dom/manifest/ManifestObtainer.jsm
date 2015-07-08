/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
 /*
 * ManifestObtainer is an implementation of:
 * http://w3c.github.io/manifest/#obtaining
 *
 * Exposes public method `.obtainManifest(Window)`, which returns
 * a promise. If successful, you get back a manifest object.
 *
 * Import it with URL:
 *   'chrome://global/content/manifestMessages.js'
 *
 * e10s IPC messaage from this components are handled by:
 *   dom/ipc/manifestMessages.js
 *
 * Which is injected into every browser instance via browser.js.
 *
 * exported ManifestObtainer
 */
/*globals Components, Task, PromiseMessage, XPCOMUtils, ManifestProcessor, BrowserUtils*/
'use strict';
const {
  utils: Cu,
  classes: Cc,
  interfaces: Ci
} = Components;
Cu.import('resource://gre/modules/Task.jsm');
Cu.import('resource://gre/modules/PromiseMessage.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/ManifestProcessor.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'BrowserUtils',  // jshint ignore:line
  'resource://gre/modules/BrowserUtils.jsm');

const processor = new ManifestProcessor();

/**
 * Asynchronously processes the result of response after having fetched
 * a manifest.
 * @param {Response} aResp Response from fetch().
 * @param {Window} aContentWindow The content window.
 * @return {Promise<Object>} The processed manifest.
 */
const processResponse = Task.async(function* (aResp, aContentWindow) {
  const badStatus = aResp.status < 200 || aResp.status >= 300;
  if (aResp.type === 'error' || badStatus) {
    const msg =
      `Fetch error: ${aResp.status} - ${aResp.statusText} at ${aResp.url}`;
    throw new Error(msg);
  }
  const text = yield aResp.text();
  const args = {
    jsonText: text,
    manifestURL: aResp.url,
    docURL: aContentWindow.location.href
  };
  const manifest = processor.process(args);
  return manifest;
});

/**
 * Asynchronously fetches a web manifest.
 * @param {Window} a The content Window from where to extract the manifest.
 * @return {Promise<Object>}
 */
const fetchManifest = Task.async(function* (aWindow) {
  if (!aWindow || aWindow.top !== aWindow) {
    let msg = 'Window must be a top-level browsing context.';
    throw new Error(msg);
  }
  const elem = aWindow.document.querySelector('link[rel~="manifest"]');
  if (!elem || !elem.getAttribute('href')) {
    let msg = `No manifest to fetch at ${aWindow.location}`;
    throw new Error(msg);
  }
  // Throws on malformed URLs
  const manifestURL = new aWindow.URL(elem.href, elem.baseURI);
  if (!canLoadManifest(elem)) {
    let msg = `Content Security Policy: The page's settings blocked the `;
    msg += `loading of a resource at ${elem.href}`;
    throw new Error(msg);
  }
  const reqInit = {
    mode: 'cors'
  };
  if (elem.crossOrigin === 'use-credentials') {
    reqInit.credentials = 'include';
  }
  const req = new aWindow.Request(manifestURL, reqInit);
  req.setContentPolicyType(Ci.nsIContentPolicy.TYPE_WEB_MANIFEST);
  const response = yield aWindow.fetch(req);
  const manifest = yield processResponse(response, aWindow);
  return manifest;
});

/**
 * Checks against security manager if we can load the web manifest.
 * @param  {HTMLLinkElement} aElem The HTML element to security check.
 * @return {Boolean} True if it can, false if it can't.
 */
function canLoadManifest(aElem) {
  const contentPolicy = Cc['@mozilla.org/layout/content-policy;1']
    .getService(Ci.nsIContentPolicy);
  const mimeType = aElem.type || 'application/manifest+json';
  const elemURI = BrowserUtils.makeURI(
    aElem.href, aElem.ownerDocument.characterSet
  );
  const shouldLoad = contentPolicy.shouldLoad(
    Ci.nsIContentPolicy.TYPE_WEB_MANIFEST, elemURI,
    aElem.ownerDocument.documentURIObject,
    aElem, mimeType, null
  );
  return shouldLoad === Ci.nsIContentPolicy.ACCEPT;
}

/**
 * ManifestObtainer
 * @constructor
 */
function ManifestObtainer() {
}

/**
 * Public interface for obtaining a web manifest.
 * @param  {XULWindow or DOMWindow} aWindow The Window from which to fetch
 *                                          the manifest.
 * @return {Promise<Object>} The processed manifest.
 */
ManifestObtainer.prototype.obtainManifest = Task.async(
  function* (aWindowOrBrowser) {
    const msgKey = 'DOM:ManifestObtainer:Obtain';
    if (!(aWindowOrBrowser && (aWindowOrBrowser.namespaceURI || aWindowOrBrowser.location))) {
      throw new TypeError('Invalid input.');
    }
    if (isXULBrowser(aWindowOrBrowser)) {
      const mm = aWindowOrBrowser.messageManager;
      const {data: {success, result}} = yield PromiseMessage.send(mm, msgKey);
      if (!success) {
        const error = toError(result);
        throw error;
      }
      return result;
    }
    const manifest = yield fetchManifest(aWindowOrBrowser);
    return manifest;
  }
);

function toError(aErrorClone) {
  let error;
  switch (aErrorClone.name) {
  case 'TypeError':
    error = new TypeError();
    break;
  default:
    error = new Error();
  }
  Object.getOwnPropertyNames(aErrorClone)
    .forEach(name => error[name] = aErrorClone[name]);
  return error;
}

function isXULBrowser(aBrowser) {
  const XUL = 'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul';
  return (aBrowser.namespaceURI && aBrowser.namespaceURI === XUL);
}

this.ManifestObtainer = ManifestObtainer; // jshint ignore:line
this.EXPORTED_SYMBOLS = ['ManifestObtainer']; // jshint ignore:line
