/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/* exported initPromise, shutdownPromise, waitForEvent, setE10sPrefs,
            unsetE10sPrefs, forceGC */

/**
 * Set e10s related preferences in the test environment.
 * @return {Promise} promise that resolves when preferences are set.
 */
function setE10sPrefs() {
  return new Promise(resolve =>
    SpecialPowers.pushPrefEnv({
      set: [
        ['browser.tabs.remote.autostart', true],
        ['browser.tabs.remote.force-enable', true],
        ['extensions.e10sBlocksEnabling', false]
      ]
    }, resolve));
}

/**
 * Unset e10s related preferences in the test environment.
 * @return {Promise} promise that resolves when preferences are unset.
 */
function unsetE10sPrefs() {
  return new Promise(resolve => {
    SpecialPowers.popPrefEnv(resolve);
  });
}

// Load the shared-head file first.
/* import-globals-from shared-head.js */
Services.scriptloader.loadSubScript(
  'chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js',
  this);

/**
 * Returns a promise that resolves when 'a11y-init-or-shutdown' event is fired.
 * @return {Promise} event promise evaluating to event's data
 */
function a11yInitOrShutdownPromise() {
  return new Promise(resolve => {
    let observe = (subject, topic, data) => {
      Services.obs.removeObserver(observe, 'a11y-init-or-shutdown');
      resolve(data);
    };
    Services.obs.addObserver(observe, 'a11y-init-or-shutdown');
  });
}

/**
 * Returns a promise that resolves when 'a11y-init-or-shutdown' event is fired
 * in content.
 * @param  {Object}   browser  current "tabbrowser" element
 * @return {Promise}  event    promise evaluating to event's data
 */
function contentA11yInitOrShutdownPromise(browser) {
  return ContentTask.spawn(browser, {}, a11yInitOrShutdownPromise);
}

/**
 * A helper function that maps 'a11y-init-or-shutdown' event to a promise that
 * resovles or rejects depending on whether accessibility service is expected to
 * be initialized or shut down.
 */
function promiseOK(promise, expected) {
  return promise.then(flag =>
    flag === expected ? Promise.resolve() : Promise.reject());
}

/**
 * Checks and returns a promise that resolves when accessibility service is
 * initialized with the correct flag.
 * @param  {?Object} contentBrowser optinal remove browser object that indicates
 *                                  that accessibility service is expected to be
 *                                  initialized in content process.
 * @return {Promise}                promise that resolves when the accessibility
 *                                  service initialized correctly.
 */
function initPromise(contentBrowser) {
  let a11yInitPromise = contentBrowser ?
    contentA11yInitOrShutdownPromise(contentBrowser) :
    a11yInitOrShutdownPromise();
  return promiseOK(a11yInitPromise, '1').then(
    () => ok(true, 'Service initialized correctly'),
    () => ok(false, 'Service shutdown incorrectly'));
}

/**
 * Checks and returns a promise that resolves when accessibility service is
 * shut down with the correct flag.
 * @param  {?Object} contentBrowser optinal remove browser object that indicates
 *                                  that accessibility service is expected to be
 *                                  shut down in content process.
 * @return {Promise}                promise that resolves when the accessibility
 *                                  service shuts down correctly.
 */
function shutdownPromise(contentBrowser) {
  let a11yShutdownPromise = contentBrowser ?
    contentA11yInitOrShutdownPromise(contentBrowser) :
    a11yInitOrShutdownPromise();
  return promiseOK(a11yShutdownPromise, '0').then(
    () => ok(true, 'Service shutdown correctly'),
    () => ok(false, 'Service initialized incorrectly'));
}

/**
 * Simpler verions of waitForEvent defined in
 * accessible/tests/browser/events.js
 */
function waitForEvent(eventType, expectedId) {
  return new Promise(resolve => {
    let eventObserver = {
      observe(subject) {
        let event = subject.QueryInterface(Ci.nsIAccessibleEvent);
        if (event.eventType === eventType &&
            event.accessible.id === expectedId) {
          Services.obs.removeObserver(this, 'accessible-event');
          resolve(event);
        }
      }
    };
    Services.obs.addObserver(eventObserver, 'accessible-event');
  });
}

/**
 * Force garbage collection.
 */
function forceGC() {
  SpecialPowers.gc();
  SpecialPowers.forceGC();
  SpecialPowers.forceCC();
  SpecialPowers.gc();
  SpecialPowers.forceGC();
  SpecialPowers.forceCC();
}
