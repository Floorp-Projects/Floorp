/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.*/
/*
 * Manifest obtainer frame script implementation of:
 * http://www.w3.org/TR/appmanifest/#obtaining
 *
 * It searches a top-level browsing context for
 * a <link rel=manifest> element. Then fetches
 * and processes the linked manifest.
 *
 * BUG: https://bugzilla.mozilla.org/show_bug.cgi?id=1083410
 */
/*globals content, sendAsyncMessage, addMessageListener, Components*/
'use strict';
const {
  utils: Cu,
  classes: Cc,
  interfaces: Ci
} = Components;
const {
  ManifestProcessor
} = Cu.import('resource://gre/modules/WebManifest.jsm', {});
const {
  Task: {
    spawn, async
  }
} = Components.utils.import('resource://gre/modules/Task.jsm', {});

addMessageListener('DOM:ManifestObtainer:Obtain', async(function* (aMsg) {
  const response = {
    msgId: aMsg.data.msgId,
    success: true,
    result: undefined
  };
  try {
    response.result = yield fetchManifest();
  } catch (err) {
    response.success = false;
    response.result = cloneError(err);
  }
  sendAsyncMessage('DOM:ManifestObtainer:Obtain', response);
}));

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
  return spawn(function* () {
    if (!content || content.top !== content) {
      let msg = 'Content window must be a top-level browsing context.';
      throw new Error(msg);
    }
    const elem = content.document.querySelector('link[rel~="manifest"]');
    if (!elem || !elem.getAttribute('href')) {
      let msg = 'No manifest to fetch.';
      throw new Error(msg);
    }
    // Throws on malformed URLs
    const manifestURL = new content.URL(elem.href, elem.baseURI);
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
    const req = new content.Request(manifestURL, reqInit);
    req.setContext('manifest');
    const response = yield content.fetch(req);
    const manifest = yield processResponse(response, content);
    return manifest;
  });
}

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

function processResponse(aResp, aContentWindow) {
  return spawn(function* () {
    const badStatus = aResp.status < 200 || aResp.status >= 300;
    if (aResp.type === 'error' || badStatus) {
      let msg =
        `Fetch error: ${aResp.status} - ${aResp.statusText} at ${aResp.url}`;
      throw new Error(msg);
    }
    const text = yield aResp.text();
    const args = {
      jsonText: text,
      manifestURL: aResp.url,
      docURL: aContentWindow.location.href
    };
    const processor = new ManifestProcessor();
    const manifest = processor.process(args);
    return Cu.cloneInto(manifest, content);
  });
}
