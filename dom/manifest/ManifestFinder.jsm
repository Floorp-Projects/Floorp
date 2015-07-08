/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* globals Components, Task, PromiseMessage */
'use strict';
const {
  utils: Cu
} = Components;
Cu.import('resource://gre/modules/PromiseMessage.jsm');
Cu.import('resource://gre/modules/Task.jsm');

/**
 * @constructor
 */
function ManifestFinder() {}

/**
 * checks if a browser window's document has a conforming
 * manifest link relationship.
 * @param aWindowOrBrowser the XUL browser or window to check.
 * @return {Promise}
 */
ManifestFinder.prototype.hasManifestLink = Task.async(
  function* (aWindowOrBrowser) {
    const msgKey = 'DOM:WebManifest:hasManifestLink';
    if (!(aWindowOrBrowser && (aWindowOrBrowser.namespaceURI || aWindowOrBrowser.location))) {
      throw new TypeError('Invalid input.');
    }
    if (isXULBrowser(aWindowOrBrowser)) {
      const mm = aWindowOrBrowser.messageManager;
      const reply = yield PromiseMessage.send(mm, msgKey);
      return reply.data.result;
    }
    return checkForManifest(aWindowOrBrowser);
  }
);

function isXULBrowser(aBrowser) {
  const XUL = 'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul';
  return (aBrowser.namespaceURI && aBrowser.namespaceURI === XUL);
}

function checkForManifest(aWindow) {
  // Only top-level browsing contexts are valid.
  if (!aWindow || aWindow.top !== aWindow) {
    return false;
  }
  const elem = aWindow.document.querySelector('link[rel~="manifest"]');
  // Only if we have an element and a non-empty href attribute.
  if (!elem || !elem.getAttribute('href')) {
    return false;
  }
  return true;
}

this.EXPORTED_SYMBOLS = [ // jshint ignore:line
  'ManifestFinder'
];
