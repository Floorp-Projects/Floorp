/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const system = require('sdk/system/events');
const { frames } = require('sdk/remote/child');

// map observer topics to tab event names
const EVENTS = {
  'content-document-interactive': 'ready',
  'chrome-document-interactive': 'ready',
  'content-document-loaded': 'load',
  'chrome-document-loaded': 'load',
// 'content-page-shown': 'pageshow', // bug 1024105
}

function topicListener({ subject, type }) {
  let window = subject.defaultView;
  if (!window)
    return;
  let frame = frames.getFrameForWindow(subject.defaultView);
  if (frame)
    frame.port.emit('sdk/tab/event', EVENTS[type]);
}

for (let topic in EVENTS)
  system.on(topic, topicListener, true);

// bug 1024105 - content-page-shown notification doesn't pass persisted param
function eventListener({target, type, persisted}) {
  let frame = this;
  if (target === frame.content.document)
    frame.port.emit('sdk/tab/event', type, persisted);
}
frames.addEventListener('pageshow', eventListener, true);
