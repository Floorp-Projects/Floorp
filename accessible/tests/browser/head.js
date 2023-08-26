/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported initAccService, shutdownAccService, waitForEvent, accConsumersChanged */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

const { CommonUtils } = ChromeUtils.importESModule(
  "chrome://mochitests/content/browser/accessible/tests/browser/Common.sys.mjs"
);

/**
 * Capture when 'a11y-consumers-changed' event is fired.
 *
 * @param  {?Object} target
 *         [optional] browser object that indicates that accessibility service
 *         is in content process.
 * @return {Array}
 *         List of promises where first one is the promise for when the event
 *         observer is added and the second one for when the event is observed.
 */
function accConsumersChanged(target) {
  return target
    ? [
        SpecialPowers.spawn(target, [], () =>
          content.CommonUtils.addAccConsumersChangedObserver()
        ),
        SpecialPowers.spawn(target, [], () =>
          content.CommonUtils.observeAccConsumersChanged()
        ),
      ]
    : [
        CommonUtils.addAccConsumersChangedObserver(),
        CommonUtils.observeAccConsumersChanged(),
      ];
}

/**
 * Capture when accessibility service is initialized.
 *
 * @param  {?Object} target
 *         [optional] browser object that indicates that accessibility service
 *         is expected to be initialized in content process.
 * @return {Array}
 *         List of promises where first one is the promise for when the event
 *         observer is added and the second one for when the event is observed.
 */
function initAccService(target) {
  return target
    ? [
        SpecialPowers.spawn(target, [], () =>
          content.CommonUtils.addAccServiceInitializedObserver()
        ),
        SpecialPowers.spawn(target, [], () =>
          content.CommonUtils.observeAccServiceInitialized()
        ),
      ]
    : [
        CommonUtils.addAccServiceInitializedObserver(),
        CommonUtils.observeAccServiceInitialized(),
      ];
}

/**
 * Capture when accessibility service is shutdown.
 *
 * @param  {?Object} target
 *         [optional] browser object that indicates that accessibility service
 *         is expected to be shutdown in content process.
 * @return {Array}
 *         List of promises where first one is the promise for when the event
 *         observer is added and the second one for when the event is observed.
 */
function shutdownAccService(target) {
  return target
    ? [
        SpecialPowers.spawn(target, [], () =>
          content.CommonUtils.addAccServiceShutdownObserver()
        ),
        SpecialPowers.spawn(target, [], () =>
          content.CommonUtils.observeAccServiceShutdown()
        ),
      ]
    : [
        CommonUtils.addAccServiceShutdownObserver(),
        CommonUtils.observeAccServiceShutdown(),
      ];
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
        let id;
        try {
          id = event.accessible.id;
        } catch (e) {
          // This can throw NS_ERROR_FAILURE.
        }
        if (event.eventType === eventType && id === expectedId) {
          Services.obs.removeObserver(this, "accessible-event");
          resolve(event);
        }
      },
    };
    Services.obs.addObserver(eventObserver, "accessible-event");
  });
}
