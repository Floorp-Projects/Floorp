/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { when } = require('sdk/system/unload');
const { process, frames } = require('sdk/remote/child');
const { loaderID } = require('@loader/options');
const { processID } = require('sdk/system/runtime');
const system = require('sdk/system/events');
const { Cu } = require('chrome');
const { isChildLoader } = require('sdk/remote/core');
const { getOuterId } = require('sdk/window/utils');

function log(str) {
  console.log("remote[" + loaderID + "][" + processID + "]: " + str);
}

log("module loaded");

process.port.emit('sdk/test/load');

process.port.on('sdk/test/ping', (process, key) => {
  log("received process ping");
  process.port.emit('sdk/test/pong', key);
});

var frameCount = 0;
frames.forEvery(frame => {
  frameCount++;
  frame.on('detach', () => {
    frameCount--;
  });

  frame.port.on('sdk/test/ping', (frame, key) => {
    log("received frame ping");
    frame.port.emit('sdk/test/pong', key);
  });
});

frames.port.on('sdk/test/checkproperties', frame => {
  frame.port.emit('sdk/test/replyproperties', {
    isTab: frame.isTab
  });
});

process.port.on('sdk/test/count', () => {
  log("received count ping");
  process.port.emit('sdk/test/count', frameCount);
});

process.port.on('sdk/test/getprocessid', () => {
  process.port.emit('sdk/test/processid', processID);
});

frames.port.on('sdk/test/testunload', (frame) => {
  // Cache the content since the frame will have been destroyed by the time
  // we see the unload event.
  let content = frame.content;
  when((reason) => {
    content.location = "#unloaded:" + reason;
  });
});

frames.port.on('sdk/test/testdetachonunload', (frame) => {
  let content = frame.content;
  frame.on('detach', () => {
    console.log("Detach from " + frame.content.location);
    frame.content.location = "#unloaded";
  });
});

frames.port.on('sdk/test/sendevent', (frame) => {
  let doc = frame.content.document;

  let listener = () => {
    frame.port.emit('sdk/test/sawreply');
  }

  system.on("Test:Reply", listener);
  let event = new frame.content.CustomEvent("Test:Event");
  doc.dispatchEvent(event);
  system.off("Test:Reply", listener);
  frame.port.emit('sdk/test/eventsent');
});

process.port.on('sdk/test/parentload', () => {
  let loaded = false;
  let message = "";
  try {
    require('sdk/remote/parent');
    loaded = true;
  }
  catch (e) {
    message = "" + e;
  }

  process.port.emit('sdk/test/parentload',
    isChildLoader,
    loaded,
    message
  )
});

function listener(event) {
  // Use the raw observer service here since it will be usable even if the
  // loader has unloaded
  let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
  Services.obs.notifyObservers(null, "Test:Reply", "");
}

frames.port.on('sdk/test/registerframesevent', (frame) => {
  frames.addEventListener("Test:Event", listener, true);
});

frames.port.on('sdk/test/unregisterframesevent', (frame) => {
  frames.removeEventListener("Test:Event", listener, true);
});

frames.port.on('sdk/test/registerframeevent', (frame) => {
  frame.addEventListener("Test:Event", listener, true);
});

frames.port.on('sdk/test/unregisterframeevent', (frame) => {
  frame.removeEventListener("Test:Event", listener, true);
});

process.port.on('sdk/test/cpow', (process, arg, cpows) => {
  process.port.emit('sdk/test/cpow', arg, getOuterId(cpows.window));
});
