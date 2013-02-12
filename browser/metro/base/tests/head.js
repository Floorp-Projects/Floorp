/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*=============================================================================
  Globals
=============================================================================*/
XPCOMUtils.defineLazyModuleGetter(this, "Promise", "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Task", "resource://gre/modules/Task.jsm");

/*=============================================================================
  Useful constants
=============================================================================*/
const serverRoot = "http://example.com/browser/metro/";
const baseURI = "http://mochi.test:8888/browser/metro/";
const chromeRoot = getRootDirectory(gTestPath);
const kDefaultWait = 10000;
const kDefaultInterval = 50;

/*=============================================================================
  Asynchronous test helpers
=============================================================================*/
/**
 *  Loads a URL in a new tab asynchronously.
 *
 * Usage:
 *    Task.spawn(function() {
 *      let tab = yield addTab("http://example.com/");
 *      ok(Browser.selectedTab == tab, "the new tab is selected");
 *    });
 *
 * @param aUrl the URL to load
 * @returns a task that resolves to the new tab object after the URL is loaded.
 */
function addTab(aUrl) {
  return Task.spawn(function() {
    info("Opening "+aUrl+" in a new tab");
    let tab = Browser.addTab(aUrl, true);
    yield waitForEvent(tab.browser, "pageshow");

    is(tab.browser.currentURI.spec, aUrl, aUrl + " is loaded");
    registerCleanupFunction(function() Browser.closeTab(tab));
    throw new Task.Result(tab);
  });
}

/**
 * Waits a specified number of miliseconds for a specified event to be
 * fired on a specified element.
 *
 * Usage:
 *    let receivedEvent = waitForEvent(element, "eventName");
 *    // Do some processing here that will cause the event to be fired
 *    // ...
 *    // Now yield until the Promise is fulfilled
 *    yield receivedEvent;
 *    if (receivedEvent && !(receivedEvent instanceof Error)) {
 *      receivedEvent.msg == "eventName";
 *      // ...
 *    }
 *
 * @param aSubject the element that should receive the event
 * @param aEventName the event to wait for
 * @param aTimeoutMs the number of miliseconds to wait before giving up
 * @returns a Promise that resolves to the received event, or to an Error
 */
function waitForEvent(aSubject, aEventName, aTimeoutMs) {
  let eventDeferred = Promise.defer();
  let timeoutMs = aTimeoutMs || kDefaultWait;
  let timerID = setTimeout(function wfe_canceller() {
    aSubject.removeEventListener(aEventName, onEvent);
    eventDeferred.reject( new Error(aEventName+" event timeout") );
  }, timeoutMs);

  function onEvent(aEvent) {
    // stop the timeout clock and resume
    clearTimeout(timerID);
    eventDeferred.resolve(aEvent);
  }

  function cleanup() {
    // unhook listener in case of success or failure
    aSubject.removeEventListener(aEventName, onEvent);
  }
  eventDeferred.promise.then(cleanup, cleanup);

  aSubject.addEventListener(aEventName, onEvent, false);
  return eventDeferred.promise;
}

/**
 * Waits a specified number of miliseconds.
 *
 * Usage:
 *    let wait = yield waitForMs(2000);
 *    ok(wait, "2 seconds should now have elapsed");
 *
 * @param aMs the number of miliseconds to wait for
 * @returns a Promise that resolves to true after the time has elapsed
 */
function waitForMs(aMs) {
  info("Wating for " + aMs + "ms");
  let deferred = Promise.defer();
  let startTime = Date.now();
  setTimeout(done, aMs);

  function done() {
    deferred.resolve(true);
    info("waitForMs finished waiting, waited for "
       + (Date.now() - startTime)
       + "ms");
  }

  return deferred.promise;
}

/**
 * Waits a specified number of miliseconds for a supplied callback to
 * return a truthy value.
 *
 * Usage:
 *     let success = yield waitForCondition(myTestFunction);
 *     if (success && !(success instanceof Error)) {
 *       // ...
 *     }
 *
 * @param aCondition the callback that must return a truthy value
 * @param aTimeoutMs the number of miliseconds to wait before giving up
 * @param aIntervalMs the number of miliseconds between calls to aCondition
 * @returns a Promise that resolves to true, or to an Error
 */
function waitForCondition(aCondition, aTimeoutMs, aIntervalMs) {
  let deferred = Promise.defer();
  let timeoutMs = aTimeoutMs || kDefaultWait;
  let intervalMs = aIntervalMs || kDefaultInterval;
  let startTime = Date.now();

  function testCondition() {
    let now = Date.now();
    if((now - startTime) > timeoutMs) {
      deferred.reject( new Error("Timed out waiting for condition to be true") );
      return;
    }

    let condition;
    try {
      condition = aCondition();
    } catch (e) {
      deferred.reject( new Error("Got exception while attempting to test conditino: " + e) );
      return;
    }

    if (condition) {
      deferred.resolve(true);
    } else {
      setTimeout(testCondition, intervalMs);
    }
  }

  setTimeout(testCondition, 0);
  return deferred.promise;
}

/*=============================================================================
  Native input synthesis helpers
=============================================================================*/
// Keyboard layouts for use with synthesizeNativeKey
const usEnglish = 0x409;
const arSpanish = 0x2C0A;

// Modifiers for use with synthesizeNativeKey
const leftShift = 0x100;
const rightShift = 0x200;
const leftControl = 0x400;
const rightControl = 0x800;
const leftAlt = 0x1000;
const rightAlt = 0x2000;

function synthesizeNativeKey(aKbLayout, aVKey, aModifiers) {
  Browser.windowUtils.sendNativeKeyEvent(aKbLayout, aVKey, aModifiers, '', '');
}

function synthesizeNativeMouse(aElement, aOffsetX, aOffsetY, aMsg) {
  let x = aOffsetX;
  let y = aOffsetY;
  if (aElement) {
    if (aElement.getBoundingClientRect) {
      let rect = aElement.getBoundingClientRect();
      x += rect.left;
      y += rect.top;
    } else if(aElement.left && aElement.top) {
      x += aElement.left;
      y += aElement.top;
    }
  }
  Browser.windowUtils.sendNativeMouseEvent(x, y, aMsg, 0, null);
}

function synthesizeNativeMouseMove(aElement, aOffsetX, aOffsetY) {
  synthesizeNativeMouse(aElement,
                        aOffsetX,
                        aOffsetY,
                        0x0001);  // MOUSEEVENTF_MOVE
}

function synthesizeNativeMouseLDown(aElement, aOffsetX, aOffsetY) {
  synthesizeNativeMouse(aElement,
                        aOffsetX,
                        aOffsetY,
                        0x0002);  // MOUSEEVENTF_LEFTDOWN
}

function synthesizeNativeMouseLUp(aElement, aOffsetX, aOffsetY) {
  synthesizeNativeMouse(aElement,
                        aOffsetX,
                        aOffsetY,
                        0x0004);  // MOUSEEVENTF_LEFTUP
}

function synthesizeNativeMouseRDown(aElement, aOffsetX, aOffsetY) {
  synthesizeNativeMouse(aElement,
                        aOffsetX,
                        aOffsetY,
                        0x0008);  // MOUSEEVENTF_RIGHTDOWN
}

function synthesizeNativeMouseRUp(aElement, aOffsetX, aOffsetY) {
  synthesizeNativeMouse(aElement,
                        aOffsetX,
                        aOffsetY,
                        0x0010);  // MOUSEEVENTF_RIGHTUP
}

function synthesizeNativeMouseMDown(aElement, aOffsetX, aOffsetY) {
  synthesizeNativeMouse(aElement,
                        aOffsetX,
                        aOffsetY,
                        0x0020);  // MOUSEEVENTF_MIDDLEDOWN
}

function synthesizeNativeMouseMUp(aElement, aOffsetX, aOffsetY) {
  synthesizeNativeMouse(aElement,
                        aOffsetX,
                        aOffsetY,
                        0x0040);  // MOUSEEVENTF_MIDDLEUP
}

/*=============================================================================
  Test-running helpers
=============================================================================*/
let gCurrentTest = null;
let gTests = [];

function runTests() {
  waitForExplicitFinish();
  Task.spawn(function() {
    while((gCurrentTest = gTests.shift())){
      info(gCurrentTest.desc);
      yield Task.spawn(gCurrentTest.run);
      info("END "+gCurrentTest.desc);
    }
    info("done with gTests while loop, calling finish");
    finish();
  });
}
