/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci } = require('chrome');
const system = require('sdk/system/events');
const { frames } = require('sdk/remote/child');
const { WorkerChild } = require('sdk/content/worker-child');

// map observer topics to tab event names
const EVENTS = {
  'content-document-global-created': 'create',
  'chrome-document-global-created': 'create',
  'content-document-interactive': 'ready',
  'chrome-document-interactive': 'ready',
  'content-document-loaded': 'load',
  'chrome-document-loaded': 'load',
// 'content-page-shown': 'pageshow', // bug 1024105
}

function topicListener({ subject, type }) {
  // NOTE detect the window from the subject:
  // - on *-global-created the subject is the window
  // - in the other cases it is the document object
  let window = subject instanceof Ci.nsIDOMWindow ? subject : subject.defaultView;
  if (!window){
    return;
  }
  let frame = frames.getFrameForWindow(window);
  if (frame) {
    let readyState = frame.content.document.readyState;
    frame.port.emit('sdk/tab/event', EVENTS[type], { readyState });
  }
}

for (let topic in EVENTS)
  system.on(topic, topicListener, true);

// bug 1024105 - content-page-shown notification doesn't pass persisted param
function eventListener({target, type, persisted}) {
  let frame = this;
  if (target === frame.content.document) {
    frame.port.emit('sdk/tab/event', type, persisted);
  }
}
frames.addEventListener('pageshow', eventListener, true);

frames.port.on('sdk/tab/attach', (frame, options) => {
  options.window = frame.content;
  new WorkerChild(options);
});

// Forward the existent frames's readyState.
for (let frame of frames) {
  let readyState = frame.content.document.readyState;
  frame.port.emit('sdk/tab/event', 'init', { readyState });
}
