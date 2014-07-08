/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// bug 673569 - let each frame script have its own anonymous scope 
(function() {

const observerSvc = Components.classes["@mozilla.org/observer-service;1"].
                    getService(Components.interfaces.nsIObserverService);

// map observer topics to tab event names
const EVENTS = {
  'content-document-interactive': 'ready',
  'chrome-document-interactive': 'ready',
  'content-document-loaded': 'load',
  'chrome-document-loaded': 'load',
// 'content-page-shown': 'pageshow', // bug 1024105
}

let listener = {
  observe: function(subject, topic) {
    // observer service keeps a strong reference to the listener, and this
    // method can get called after the tab is closed, so we should remove it.
    if (!docShell) {
      observerSvc.removeObserver(this, topic);
    }
    else {
      if (subject === content.document)
        sendAsyncMessage('sdk/tab/event', { type: EVENTS[topic] });
    }
  }
}

Object.keys(EVENTS).forEach( (topic) =>
  observerSvc.addObserver(listener, topic, false));

// bug 1024105 - content-page-shown notification doesn't pass persisted param
docShell.chromeEventHandler.addEventListener('pageshow', (e) => {
  if (e.target === content.document)
    sendAsyncMessage('sdk/tab/event', { type: e.type, persisted: e.persisted });
}, true);

})();
