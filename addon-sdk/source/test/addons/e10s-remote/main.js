/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const LOCAL_URI = "about:robots";
const REMOTE_URI = "data:text/html;charset=utf-8,remote";

const { Cu } = require('chrome');
const { Loader } = require('sdk/test/loader');
const { getTabs, openTab, closeTab, setTabURL, getBrowserForTab, getURI } = require('sdk/tabs/utils');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { cleanUI } = require("sdk/test/utils");
const { setTimeout } = require("sdk/timers");
const { promiseEvent, promiseDOMEvent, promiseEventOnItemAndContainer,
        waitForProcesses, getChildFrameCount, isE10S } = require("./utils");
const { after } = require('sdk/test/utils');
const { processID } = require('sdk/system/runtime');

const { set } = require('sdk/preferences/service');
// The hidden preload browser messes up our frame counts
set('browser.newtab.preload', false);

function promiseTabFrameAttach(frames) {
  return new Promise(resolve => {
    let listener = function(frame, ...args) {
      if (!frame.isTab)
        return;
      frames.off("attach", listener);
      resolve([frame, ...args]);
    }

    frames.on("attach", listener);
  });
}

// Check that we see a process stop and start
exports["test process restart"] = function*(assert) {
  if (!isE10S) {
    assert.pass("Skipping test in non-e10s mode");
    return;
  }

  let window = getMostRecentBrowserWindow();

  let tabs = getTabs(window);
  assert.equal(tabs.length, 1, "Should have just the one tab to start with");
  let tab = tabs[0];
  let browser = getBrowserForTab(tab);

  let loader = new Loader(module);
  let { processes, frames } = yield waitForProcesses(loader);

  let remoteProcess = Array.filter(processes, p => p.isRemote)[0];
  let localProcess = Array.filter(processes, p => !p.isRemote)[0];
  let remoteFrame = Array.filter(frames, f => f.process == remoteProcess)[0];

  // Switch the remote tab to a local URI which should kill the remote process

  let frameDetach = promiseEventOnItemAndContainer(assert, remoteFrame, frames, 'detach');
  let frameAttach = promiseTabFrameAttach(frames);
  let processDetach = promiseEventOnItemAndContainer(assert, remoteProcess, processes, 'detach');
  let browserLoad = promiseDOMEvent(browser, "load", true);
  setTabURL(tab, LOCAL_URI);
  // The load should kill the remote frame
  yield frameDetach;
  // And create a new frame in the local process
  let [newFrame] = yield frameAttach;
  assert.equal(newFrame.process, localProcess, "New frame should be in the local process");
  // And kill the process
  yield processDetach;
  yield browserLoad;

  frameDetach = promiseEventOnItemAndContainer(assert, newFrame, frames, 'detach');
  let processAttach = promiseEvent(processes, 'attach');
  frameAttach = promiseTabFrameAttach(frames);
  browserLoad = promiseDOMEvent(browser, "load", true);
  setTabURL(tab, REMOTE_URI);
  // The load should kill the remote frame
  yield frameDetach;
  // And create a new remote process
  [remoteProcess] = yield processAttach;
  assert.ok(remoteProcess.isRemote, "Process should be remote");
  // And create a new frame in the remote process
  [newFrame] = yield frameAttach;
  assert.equal(newFrame.process, remoteProcess, "New frame should be in the remote process");
  yield browserLoad;

  browserLoad = promiseDOMEvent(browser, "load", true);
  setTabURL(tab, "about:blank");
  yield browserLoad;

  loader.unload();
};

// Test that we find the right number of processes and that messaging between
// them works and none of the streams cross
exports["test process list"] = function*(assert) {
  let loader = new Loader(module);
  let { processes } = loader.require('sdk/remote/parent');

  let processCount = 0;
  processes.forEvery(processes => processCount++);

  yield waitForProcesses(loader);

  let remoteProcesses = Array.filter(processes, process => process.isRemote);
  let localProcesses = Array.filter(processes, process => !process.isRemote);

  assert.equal(localProcesses.length, 1, "Should always be one process");

  if (isE10S) {
    assert.equal(remoteProcesses.length, 1, "Should be one remote process");
  }
  else {
    assert.equal(remoteProcesses.length, 0, "Should be no remote processes");
  }

  assert.equal(processCount, processes.length, "Should have seen all processes");

  processCount = 0;
  processes.forEvery(process => processCount++);

  assert.equal(processCount, processes.length, "forEvery should send existing processes to the listener");

  localProcesses[0].port.on('sdk/test/pong', (process, key) => {
    assert.equal(key, "local", "Should not have seen a pong from the local process with the wrong key");
  });

  if (isE10S) {
    remoteProcesses[0].port.on('sdk/test/pong', (process, key) => {
      assert.equal(key, "remote", "Should not have seen a pong from the remote process with the wrong key");
    });
  }

  let promise = promiseEventOnItemAndContainer(assert, localProcesses[0].port, processes.port, 'sdk/test/pong', localProcesses[0]);
  localProcesses[0].port.emit('sdk/test/ping', "local");

  let reply = yield promise;
  assert.equal(reply[0], "local", "Saw the process reply with the right key");

  if (isE10S) {
    promise = promiseEventOnItemAndContainer(assert, remoteProcesses[0].port, processes.port, 'sdk/test/pong', remoteProcesses[0]);
    remoteProcesses[0].port.emit('sdk/test/ping', "remote");

    reply = yield promise;
    assert.equal(reply[0], "remote", "Saw the process reply with the right key");

    assert.notEqual(localProcesses[0], remoteProcesses[0], "Processes should be different");
  }

  loader.unload();
};

// Test that the frame lists are kept up to date
exports["test frame list"] = function*(assert) {
  function browserFrames(list) {
    return Array.filter(list, b => b.isTab).length;
  }

  let window = getMostRecentBrowserWindow();

  let tabs = getTabs(window);
  assert.equal(tabs.length, 1, "Should have just the one tab to start with");

  let loader = new Loader(module);
  let { processes, frames } = yield waitForProcesses(loader);

  assert.equal(browserFrames(frames), getTabs(window).length, "Should be the right number of browser frames.");
  assert.equal((yield getChildFrameCount(processes)), frames.length, "Child processes should have the right number of frames");

  let promise = promiseTabFrameAttach(frames);
  let tab1 = openTab(window, LOCAL_URI);
  let [frame1] = yield promise;
  assert.ok(!!frame1, "Should have seen the new frame");
  assert.ok(!frame1.process.isRemote, "Frame should not be remote");

  assert.equal(browserFrames(frames), getTabs(window).length, "Should be the right number of browser frames.");
  assert.equal((yield getChildFrameCount(processes)), frames.length, "Child processes should have the right number of frames");

  promise = promiseTabFrameAttach(frames);
  let tab2 = openTab(window, REMOTE_URI);
  let [frame2] = yield promise;
  assert.ok(!!frame2, "Should have seen the new frame");
  if (isE10S)
    assert.ok(frame2.process.isRemote, "Frame should be remote");
  else
    assert.ok(!frame2.process.isRemote, "Frame should not be remote");

  assert.equal(browserFrames(frames), getTabs(window).length, "Should be the right number of browser frames.");
  assert.equal((yield getChildFrameCount(processes)), frames.length, "Child processes should have the right number of frames");

  frames.port.emit('sdk/test/ping')
  yield new Promise(resolve => {
    let count = 0;
    let listener = () => {
      console.log("Saw pong");
      count++;
      if (count == frames.length) {
        frames.port.off('sdk/test/pong', listener);
        resolve();
      }
    };
    frames.port.on('sdk/test/pong', listener);
  });

  let badListener = () => {
    assert.fail("Should not have seen a response through this frame");
  }
  frame1.port.on('sdk/test/pong', badListener);
  frame2.port.emit('sdk/test/ping', 'b');
  let [key] = yield promiseEventOnItemAndContainer(assert, frame2.port, frames.port, 'sdk/test/pong', frame2);
  assert.equal(key, 'b', "Should have seen the right response");
  frame1.port.off('sdk/test/pong', badListener);

  frame2.port.on('sdk/test/pong', badListener);
  frame1.port.emit('sdk/test/ping', 'b');
  [key] = yield promiseEventOnItemAndContainer(assert, frame1.port, frames.port, 'sdk/test/pong', frame1);
  assert.equal(key, 'b', "Should have seen the right response");
  frame2.port.off('sdk/test/pong', badListener);

  promise = promiseEventOnItemAndContainer(assert, frame1, frames, 'detach');
  closeTab(tab1);
  yield promise;

  assert.equal(browserFrames(frames), getTabs(window).length, "Should be the right number of browser frames.");
  assert.equal((yield getChildFrameCount(processes)), frames.length, "Child processes should have the right number of frames");

  promise = promiseEventOnItemAndContainer(assert, frame2, frames, 'detach');
  closeTab(tab2);
  yield promise;

  assert.equal(browserFrames(frames), getTabs(window).length, "Should be the right number of browser frames.");
  assert.equal((yield getChildFrameCount(processes)), frames.length, "Child processes should have the right number of frames");

  loader.unload();
};

// Test that multiple loaders get their own loaders in the child and messages
// don't cross. Unload should work
exports["test new loader"] = function*(assert) {
  let loader1 = new Loader(module);
  let { processes: processes1 } = yield waitForProcesses(loader1);

  let loader2 = new Loader(module);
  let { processes: processes2 } = yield waitForProcesses(loader2);

  let process1 = [...processes1][0];
  let process2 = [...processes2][0];

  process1.port.on('sdk/test/pong', (process, key) => {
    assert.equal(key, "a", "Should have seen the right pong");
  });

  process2.port.on('sdk/test/pong', (process, key) => {
    assert.equal(key, "b", "Should have seen the right pong");
  });

  process1.port.emit('sdk/test/ping', 'a');
  yield promiseEvent(process1.port, 'sdk/test/pong');

  process2.port.emit('sdk/test/ping', 'b');
  yield promiseEvent(process2.port, 'sdk/test/pong');

  loader1.unload();

  process2.port.emit('sdk/test/ping', 'b');
  yield promiseEvent(process2.port, 'sdk/test/pong');

  loader2.unload();
};

// Test that unloading the loader unloads the child instances
exports["test unload"] = function*(assert) {
  let window = getMostRecentBrowserWindow();
  let loader = new Loader(module);
  let { frames } = yield waitForProcesses(loader);

  let promise = promiseTabFrameAttach(frames);
  let tab = openTab(window, "data:,<html/>");
  let browser = getBrowserForTab(tab);
  yield promiseDOMEvent(browser, "load", true);
  let [frame] = yield promise;
  assert.ok(!!frame, "Should have seen the new frame");

  promise = promiseDOMEvent(browser, 'hashchange');
  frame.port.emit('sdk/test/testunload');
  loader.unload("shutdown");
  yield promise;

  let hash = getURI(tab).replace(/.*#/, "");
  assert.equal(hash, "unloaded:shutdown", "Saw the correct hash change.")

  closeTab(tab);
}

// Test that unloading the loader causes the child to see frame detach events
exports["test frame detach on unload"] = function*(assert) {
  let window = getMostRecentBrowserWindow();
  let loader = new Loader(module);
  let { frames } = yield waitForProcesses(loader);

  let promise = promiseTabFrameAttach(frames);
  let tab = openTab(window, "data:,<html/>");
  let browser = getBrowserForTab(tab);
  yield promiseDOMEvent(browser, "load", true);
  let [frame] = yield promise;
  assert.ok(!!frame, "Should have seen the new frame");

  promise = promiseDOMEvent(browser, 'hashchange');
  frame.port.emit('sdk/test/testdetachonunload');
  loader.unload("shutdown");
  yield promise;

  let hash = getURI(tab).replace(/.*#/, "");
  assert.equal(hash, "unloaded", "Saw the correct hash change.")

  closeTab(tab);
}

// Test that DOM event listener on the frame object works
exports["test frame event listeners"] = function*(assert) {
  let window = getMostRecentBrowserWindow();
  let loader = new Loader(module);
  let { frames } = yield waitForProcesses(loader);

  let promise = promiseTabFrameAttach(frames);
  let tab = openTab(window, "data:text/html,<html></html>");
  let browser = getBrowserForTab(tab);
  yield promiseDOMEvent(browser, "load", true);
  let [frame] = yield promise;
  assert.ok(!!frame, "Should have seen the new frame");

  frame.port.emit('sdk/test/registerframeevent');
  promise = Promise.all([
    promiseEvent(frame.port, 'sdk/test/sawreply'),
    promiseEvent(frame.port, 'sdk/test/eventsent')
  ]);

  frame.port.emit('sdk/test/sendevent');
  yield promise;

  frame.port.emit('sdk/test/unregisterframeevent');
  promise = promiseEvent(frame.port, 'sdk/test/eventsent');
  frame.port.on('sdk/test/sawreply', () => {
    assert.fail("Should not have seen the event listener reply");
  });

  frame.port.emit('sdk/test/sendevent');
  yield promise;

  closeTab(tab);
  loader.unload();
}

// Test that DOM event listener on the frames object works
exports["test frames event listeners"] = function*(assert) {
  let window = getMostRecentBrowserWindow();
  let loader = new Loader(module);
  let { frames } = yield waitForProcesses(loader);

  let promise = promiseTabFrameAttach(frames);
  let tab = openTab(window, "data:text/html,<html></html>");
  let browser = getBrowserForTab(tab);
  yield promiseDOMEvent(browser, "load", true);
  let [frame] = yield promise;
  assert.ok(!!frame, "Should have seen the new frame");

  frame.port.emit('sdk/test/registerframesevent');
  promise = Promise.all([
    promiseEvent(frame.port, 'sdk/test/sawreply'),
    promiseEvent(frame.port, 'sdk/test/eventsent')
  ]);

  frame.port.emit('sdk/test/sendevent');
  yield promise;

  frame.port.emit('sdk/test/unregisterframesevent');
  promise = promiseEvent(frame.port, 'sdk/test/eventsent');
  frame.port.on('sdk/test/sawreply', () => {
    assert.fail("Should not have seen the event listener reply");
  });

  frame.port.emit('sdk/test/sendevent');
  yield promise;

  closeTab(tab);
  loader.unload();
}

// Test that unloading unregisters frame DOM events
exports["test unload removes frame event listeners"] = function*(assert) {
  let window = getMostRecentBrowserWindow();
  let loader = new Loader(module);
  let { frames } = yield waitForProcesses(loader);

  let loader2 = new Loader(module);
  let { frames: frames2 } = yield waitForProcesses(loader2);

  let promise = promiseTabFrameAttach(frames);
  let promise2 = promiseTabFrameAttach(frames2);
  let tab = openTab(window, "data:text/html,<html></html>");
  let browser = getBrowserForTab(tab);
  yield promiseDOMEvent(browser, "load", true);
  let [frame] = yield promise;
  let [frame2] = yield promise2;
  assert.ok(!!frame && !!frame2, "Should have seen the new frame");

  frame.port.emit('sdk/test/registerframeevent');
  promise = Promise.all([
    promiseEvent(frame2.port, 'sdk/test/sawreply'),
    promiseEvent(frame2.port, 'sdk/test/eventsent')
  ]);

  frame2.port.emit('sdk/test/sendevent');
  yield promise;

  loader.unload();

  promise = promiseEvent(frame2.port, 'sdk/test/eventsent');
  frame2.port.on('sdk/test/sawreply', () => {
    assert.fail("Should not have seen the event listener reply");
  });

  frame2.port.emit('sdk/test/sendevent');
  yield promise;

  closeTab(tab);
  loader2.unload();
}

// Test that unloading unregisters frames DOM events
exports["test unload removes frames event listeners"] = function*(assert) {
  let window = getMostRecentBrowserWindow();
  let loader = new Loader(module);
  let { frames } = yield waitForProcesses(loader);

  let loader2 = new Loader(module);
  let { frames: frames2 } = yield waitForProcesses(loader2);

  let promise = promiseTabFrameAttach(frames);
  let promise2 = promiseTabFrameAttach(frames2);
  let tab = openTab(window, "data:text/html,<html></html>");
  let browser = getBrowserForTab(tab);
  yield promiseDOMEvent(browser, "load", true);
  let [frame] = yield promise;
  let [frame2] = yield promise2;
  assert.ok(!!frame && !!frame2, "Should have seen the new frame");

  frame.port.emit('sdk/test/registerframesevent');
  promise = Promise.all([
    promiseEvent(frame2.port, 'sdk/test/sawreply'),
    promiseEvent(frame2.port, 'sdk/test/eventsent')
  ]);

  frame2.port.emit('sdk/test/sendevent');
  yield promise;

  loader.unload();

  promise = promiseEvent(frame2.port, 'sdk/test/eventsent');
  frame2.port.on('sdk/test/sawreply', () => {
    assert.fail("Should not have seen the event listener reply");
  });

  frame2.port.emit('sdk/test/sendevent');
  yield promise;

  closeTab(tab);
  loader2.unload();
}

// Check that the child frame has the right properties
exports["test frame properties"] = function*(assert) {
  let loader = new Loader(module);
  let { frames } = yield waitForProcesses(loader);

  let promise = new Promise(resolve => {
    let count = frames.length;
    let listener = (frame, properties) => {
      assert.equal(properties.isTab, frame.isTab,
                   "Child frame should have the same isTab property");

      if (--count == 0) {
        frames.port.off('sdk/test/replyproperties', listener);
        resolve();
      }
    }

    frames.port.on('sdk/test/replyproperties', listener);
  })

  frames.port.emit('sdk/test/checkproperties');
  yield promise;

  loader.unload();
}

// Check that non-remote processes have the same process ID and remote processes
// have different IDs
exports["test processID"] = function*(assert) {
  let loader = new Loader(module);
  let { processes } = yield waitForProcesses(loader);

  for (let process of processes) {
    process.port.emit('sdk/test/getprocessid');
    let [p, ID] = yield promiseEvent(process.port, 'sdk/test/processid');
    if (process.isRemote) {
      assert.notEqual(ID, processID, "Remote processes should have a different process ID");
    }
    else {
      assert.equal(ID, processID, "Remote processes should have the same process ID");
    }
  }

  loader.unload();
}

// Check that sdk/remote/parent and sdk/remote/child can only be loaded in the
// appropriate loaders
exports["test cannot load in wrong loader"] = function*(assert) {
  let loader = new Loader(module);
  let { processes } = yield waitForProcesses(loader);

  try {
    require('sdk/remote/child');
    assert.fail("Should not have been able to load sdk/remote/child");
  }
  catch (e) {
    assert.ok(/Cannot load sdk\/remote\/child in a main process loader/.test(e),
              "Should have seen the right exception.");
  }

  for (let process of processes) {
    processes.port.emit('sdk/test/parentload');
    let [_, isChildLoader, loaded, message] = yield promiseEvent(processes.port, 'sdk/test/parentload');
    assert.ok(isChildLoader, "Process should see itself in a child loader.");
    assert.ok(!loaded, "Process couldn't load sdk/remote/parent.");
    assert.ok(/Cannot load sdk\/remote\/parent in a child loader/.test(message),
              "Should have seen the right exception.");
  }

  loader.unload();
};

exports["test send cpow"] = function*(assert) {
  if (!isE10S) {
    assert.pass("Skipping test in non-e10s mode");
    return;
  }

  let window = getMostRecentBrowserWindow();

  let tabs = getTabs(window);
  assert.equal(tabs.length, 1, "Should have just the one tab to start with");
  let tab = tabs[0];
  let browser = getBrowserForTab(tab);

  assert.ok(Cu.isCrossProcessWrapper(browser.contentWindow),
            "Should have a CPOW for the browser content window");

  let loader = new Loader(module);
  let { processes } = yield waitForProcesses(loader);

  processes.port.emitCPOW('sdk/test/cpow', ['foobar'], { window: browser.contentWindow });
  let [process, arg, id] = yield promiseEvent(processes.port, 'sdk/test/cpow');

  assert.ok(process.isRemote, "Response should come from the remote process");
  assert.equal(arg, "foobar", "Argument should have passed through");
  assert.equal(id, browser.outerWindowID, "Should have got the ID from the child");
};

after(exports, function*(name, assert) {
  yield cleanUI();
});

require('sdk/test/runner').runTestsFromModule(module);
