"use strict";

this.EXPORTED_SYMBOLS = [
  "BrowserUITestUtils",
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Timer.jsm");


/**
 * Default wait period in millseconds, when waiting for the expected event to occur.
 * @type {number}
 */
const DEFAULT_WAIT = 2000;


/**
 * Test utility functions for dealing with the browser UI DOM.
 */
this.BrowserUITestUtils = {

  /**
   * Waits a specified number of miliseconds for a specified event to be
   * fired on a specified element.
   *
   * Usage:
   *    let receivedEvent = BrowserUITestUtils.waitForEvent(element, "eventName");
   *    // Do some processing here that will cause the event to be fired
   *    // ...
   *    // Now yield until the Promise is fulfilled
   *    yield receivedEvent;
   *    if (receivedEvent && !(receivedEvent instanceof Error)) {
   *      receivedEvent.msg == "eventName";
   *      // ...
   *    }
   *
   * @param {Element} subject - The element that should receive the event.
   * @param {string} eventName - The event to wait for.
   * @param {number} timeoutMs - The number of miliseconds to wait before giving up.
   * @param {Element} target - Expected target of the event.
   * @returns {Promise} A Promise that resolves to the received event, or
   *                    rejects with an Error.
   */
  waitForEvent(subject, eventName, timeoutMs, target) {
    return new Promise((resolve, reject) => {
      function listener(event) {
        if (target && target !== event.target) {
          return;
        }

        subject.removeEventListener(eventName, listener);
        clearTimeout(timerID);
        resolve(event);
      }

      timeoutMs = timeoutMs || DEFAULT_WAIT;
      let stack = new Error().stack;

      let timerID = setTimeout(() => {
        subject.removeEventListener(eventName, listener);
        reject(new Error(`${eventName} event timeout at ${stack}`));
      }, timeoutMs);


      subject.addEventListener(eventName, listener);
    });
  },
};
