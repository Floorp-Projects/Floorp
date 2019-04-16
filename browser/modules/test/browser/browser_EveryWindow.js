/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable mozilla/no-arbitrary-setTimeout */

const {EveryWindow} = ChromeUtils.import("resource:///modules/EveryWindow.jsm");

async function windowInited(aId, aWin) {
  // TestUtils.topicObserved returns [subject, data]. We return the
  // subject, which in this case is the window.
  return (await TestUtils.topicObserved(`${aId}:init`, (win) => {
    return aWin ? win == aWin : true;
  }))[0];
}

function windowUninited(aId, aWin, aClosing) {
  return TestUtils.topicObserved(`${aId}:uninit`, (win, closing) => {
    if (aWin && aWin != win) {
      return false;
    }
    if (!aWin) {
      return true;
    }
    if (!!aClosing != !!closing) {
      return false;
    }
    return true;
  });
}

function registerEWCallback(id) {
  EveryWindow.registerCallback(
    id,
    (win) => {
      Services.obs.notifyObservers(win, `${id}:init`);
    },
    (win, closing) => {
      Services.obs.notifyObservers(win, `${id}:uninit`, closing);
    },
  );
}

function unregisterEWCallback(id, aCallUninit) {
  EveryWindow.unregisterCallback(id, aCallUninit);
}

add_task(async function test_stuff() {
  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  let win3 = await BrowserTestUtils.openNewBrowserWindow();

  let callbackId1 = "EveryWindow:test:1";
  let callbackId2 = "EveryWindow:test:2";

  let initPromise = Promise.all([windowInited(callbackId1, window),
                                windowInited(callbackId1, win2),
                                windowInited(callbackId1, win3),
                                windowInited(callbackId2, window),
                                windowInited(callbackId2, win2),
                                windowInited(callbackId2, win3)]);

  registerEWCallback(callbackId1);
  registerEWCallback(callbackId2);

  await initPromise;
  ok(true, "Init called for all existing windows for all registered consumers");

  let uninitPromise = Promise.all([windowUninited(callbackId1, window, false),
                                  windowUninited(callbackId1, win2, false),
                                  windowUninited(callbackId1, win3, false),
                                  windowUninited(callbackId2, window, false),
                                  windowUninited(callbackId2, win2, false),
                                  windowUninited(callbackId2, win3, false)]);

  unregisterEWCallback(callbackId1);
  unregisterEWCallback(callbackId2);
  await uninitPromise;
  ok(true, "Uninit called for all existing windows");

  initPromise = Promise.all([windowInited(callbackId1, window),
                            windowInited(callbackId1, win2),
                            windowInited(callbackId1, win3),
                            windowInited(callbackId2, window),
                            windowInited(callbackId2, win2),
                            windowInited(callbackId2, win3)]);

  registerEWCallback(callbackId1);
  registerEWCallback(callbackId2);

  await initPromise;
  ok(true, "Init called for all existing windows for all registered consumers");

  uninitPromise = Promise.all([windowUninited(callbackId1, win2, true),
                              windowUninited(callbackId2, win2, true)]);
  await BrowserTestUtils.closeWindow(win2);
  await uninitPromise;
  ok(true, "Uninit called with closing=true for win2 for all registered consumers");

  uninitPromise = Promise.all([windowUninited(callbackId1, win3, true),
                              windowUninited(callbackId2, win3, true)]);
  await BrowserTestUtils.closeWindow(win3);
  await uninitPromise;
  ok(true, "Uninit called with closing=true for win3 for all registered consumers");

  initPromise = windowInited(callbackId1);
  let initPromise2 = windowInited(callbackId2);
  win2 = await BrowserTestUtils.openNewBrowserWindow();
  is(await initPromise, win2, "Init called for new window for callback 1");
  is(await initPromise2, win2, "Init called for new window for callback 2");

  uninitPromise = Promise.all([windowUninited(callbackId1, win2, true),
                              windowUninited(callbackId2, win2, true)]);
  await BrowserTestUtils.closeWindow(win2);
  await uninitPromise;
  ok(true, "Uninit called with closing=true for win2 for all registered consumers");

  uninitPromise = windowUninited(callbackId1, window, false);
  unregisterEWCallback(callbackId1);
  await uninitPromise;
  ok(true, "Uninit called for main window without closing flag for the unregistered consumer");

  uninitPromise = windowUninited(callbackId2, window, false);
  let timeoutPromise = new Promise(resolve => setTimeout(resolve, 500));
  unregisterEWCallback(callbackId2, false);
  let result = await Promise.race([uninitPromise, timeoutPromise]);
  is(result, undefined, "Uninit not called when unregistering a consumer with aCallUninit=false");
});
