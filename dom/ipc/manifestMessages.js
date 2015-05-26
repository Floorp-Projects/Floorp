/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.*/
/*
 * Manifest obtainer frame script implementation of:
 * http://w3c.github.io/manifest/#obtaining
 *
 * It searches a top-level browsing context for
 * a <link rel=manifest> element. Then fetches
 * and processes the linked manifest.
 *
 * BUG: https://bugzilla.mozilla.org/show_bug.cgi?id=1083410
 * exported ManifestObtainer
 */
/*globals content, ManifestProcessor, XPCOMUtils, sendAsyncMessage, addMessageListener, Components*/
'use strict';
const {
  utils: Cu
} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

XPCOMUtils.defineLazyModuleGetter(this, 'ManifestProcessor',
  'resource://gre/modules/ManifestProcessor.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'ManifestObtainer',
  'resource://gre/modules/ManifestObtainer.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'BrowserUtils',
  'resource://gre/modules/BrowserUtils.jsm');

addMessageListener('DOM:ManifestObtainer:Obtain', (aMsg) => {
  fetchManifest()
    .then(
      manifest => sendAsyncMessage('DOM:ManifestObtainer:Obtain', {
        success: true,
        result: manifest,
        msgId: aMsg.data.msgId
      }),
      error => sendAsyncMessage('DOM:ManifestObtainer:Obtain', {
        success: false,
        result: cloneError(error),
        msgId: aMsg.data.msgId
      })
    );
});

function cloneError(aError) {
  const clone = {
    'fileName': String(aError.fileName),
    'lineNumber': String(aError.lineNumber),
    'columnNumber': String(aError.columnNumber),
    'stack': String(aError.stack),
    'message': String(aError.message),
    'name': String(aError.name)
  };
  return clone;
}

function fetchManifest() {
  const manifestQuery = 'link[rel~="manifest"]';
  return new Promise((resolve, reject) => {
    if (!content || content.top !== content) {
      let msg = 'Content window must be a top-level browsing context.';
      return reject(new Error(msg));
    }
    const elem = content.document.querySelector(manifestQuery);
    if (!elem || !elem.getAttribute('href')) {
      let msg = 'No manifest to fetch.';
      return reject(new Error(msg));
    }
    // Will throw on "about:blank" and possibly other invalid URIs.
    const manifestURL = new content.URL(elem.href, elem.baseURI);
    const reqInit = {
      mode: 'cors'
    };
    if (elem.crossOrigin === 'use-credentials') {
      reqInit.credentials = 'include';
    }
    const req = new content.Request(manifestURL, reqInit);
    req.setContext('manifest');
    content
      .fetch(req)
      .then(resp => processResponse(resp, content))
      .then(resolve)
      .catch(reject);
  });
}

function processResponse(aResp, aContentWindow) {
  const manifestURL = aResp.url;
  return new Promise((resolve, reject) => {
    const badStatus = aResp.status < 200 || aResp.status >= 300;
    if (aResp.type === 'error' || badStatus) {
      let msg =
        `Fetch error: ${aResp.status} - ${aResp.statusText} at ${aResp.url}`;
      return reject(new Error(msg));
    }
    aResp
      .text()
      .then((text) => {
        const args = {
          jsonText: text,
          manifestURL: manifestURL,
          docURL: aContentWindow.location.href
        };
        const processor = new ManifestProcessor();
        const manifest = processor.process(args);
        resolve(Cu.cloneInto(manifest, content));
      }, reject);
  });
}
