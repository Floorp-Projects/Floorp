/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require('chrome');
const { Task: { async } } = Cu.import('resource://gre/modules/Task.jsm', {});
const { getMostRecentBrowserWindow } = require('sdk/window/utils');

const REMOTE_MODULE = "./remote-module";

function promiseEvent(emitter, event) {
  console.log("Waiting for " + event);
  return new Promise(resolve => {
    emitter.once(event, (...args) => {
      console.log("Saw " + event);
      resolve(args);
    });
  });
}
exports.promiseEvent = promiseEvent;

function promiseDOMEvent(target, event, isCapturing = false) {
  console.log("Waiting for " + event);
  return new Promise(resolve => {
    let listener = (event) => {
      target.removeEventListener(event, listener, isCapturing);
      resolve(event);
    };
    target.addEventListener(event, listener, isCapturing);
  })
}
exports.promiseDOMEvent = promiseDOMEvent;

const promiseEventOnItemAndContainer = async(function*(assert, itemport, container, event, item = itemport) {
  let itemEvent = promiseEvent(itemport, event);
  let containerEvent = promiseEvent(container, event);

  let itemArgs = yield itemEvent;
  let containerArgs = yield containerEvent;

  assert.equal(containerArgs[0], item, "Should have seen a container event for the right item");
  assert.equal(JSON.stringify(itemArgs), JSON.stringify(containerArgs), "Arguments should have matched");

  // Strip off the item from the returned arguments
  return itemArgs.slice(1);
});
exports.promiseEventOnItemAndContainer = promiseEventOnItemAndContainer;

const waitForProcesses = async(function*(loader) {
  console.log("Starting remote");
  let { processes, frames, remoteRequire } = loader.require('sdk/remote/parent');
  remoteRequire(REMOTE_MODULE, module);

  let events = [];

  // In e10s we should expect to see two processes
  let expectedCount = isE10S ? 2 : 1;

  yield new Promise(resolve => {
    let count = 0;

    // Wait for a process to be detected
    let listener = process => {
      console.log("Saw a process attach");
      // Wait for the remote module to load in this process
      process.port.once('sdk/test/load', () => {
        console.log("Saw a remote module load");
        count++;
        if (count == expectedCount) {
          processes.off('attach', listener);
          resolve();
        }
      });
    }
    processes.on('attach', listener);
  });

  console.log("Remote ready");
  return { processes, frames, remoteRequire };
});
exports.waitForProcesses = waitForProcesses;

// Counts the frames in all the child processes
const getChildFrameCount = async(function*(processes) {
  let frameCount = 0;

  for (let process of processes) {
    process.port.emit('sdk/test/count');
    let [p, count] = yield promiseEvent(process.port, 'sdk/test/count');
    frameCount += count;
  }

  return frameCount;
});
exports.getChildFrameCount = getChildFrameCount;

const mainWindow = getMostRecentBrowserWindow();
const isE10S = mainWindow.gMultiProcessBrowser;
exports.isE10S = isE10S;

if (isE10S) {
  console.log("Testing in E10S mode");
  // We expect a child process to already be present, make sure that is the case
  mainWindow.XULBrowserWindow.forceInitialBrowserRemote();
}
else {
  console.log("Testing in non-E10S mode");
}
