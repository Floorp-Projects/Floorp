/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const extensionTargetDetails = {
  // actor ID for this extention.
  actor: PropTypes.string.isRequired,
  location: PropTypes.string.isRequired,
  // manifestURL points to the manifest.json file. This URL is only valid when debugging
  // local extensions so it might be null.
  manifestURL: PropTypes.string,
  // unique extension id.
  uuid: PropTypes.string.isRequired,
};

const tabTargetDetails = {
  // the url of the tab.
  url: PropTypes.string.isRequired,
};

const workerTargetDetails = {
  // (service worker specific) one of "LISTENING", "NOT_LISTENING". undefined otherwise.
  fetch: PropTypes.string,
  // (service worker specific) true if they reached the activated state.
  isActive: PropTypes.bool,
  // (service worker specific) true if they are currently running.
  isRunning: PropTypes.bool,
  // actor id for the ServiceWorkerRegistration related to this service worker.
  registrationActor: PropTypes.string,
  // (service worker specific) scope of the service worker registration.
  scope: PropTypes.string,
  // (service worker specific) one of "RUNNING", "REGISTERING", "STOPPED".
  status: PropTypes.string,
};

const debugTarget = {
  // details property will contain a type-specific object.
  details: PropTypes.oneOfType([
    PropTypes.shape(extensionTargetDetails),
    PropTypes.shape(tabTargetDetails),
    PropTypes.shape(workerTargetDetails),
  ]).isRequired,
  // icon to display for the debug target.
  icon: PropTypes.string.isRequired,
  // unique id for the target (unique in the scope of the application lifecycle).
  // - extensions: {String} extension id (for instance "someextension@mozilla.org")
  // - tabs: {Number} outerWindowID
  // - workers: {String} id for the WorkerTargetActor corresponding to the worker
  id: PropTypes.oneOfType([PropTypes.number, PropTypes.string]).isRequired,
  // display name for the debug target.
  name: PropTypes.string.isRequired,
  // one of "EXTENSION", "TAB", "WORKER".
  type: PropTypes.string.isRequired,
};

exports.debugTarget = PropTypes.shape(debugTarget);
