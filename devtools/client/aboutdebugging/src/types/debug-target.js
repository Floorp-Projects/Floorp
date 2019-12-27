/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { DEBUG_TARGETS } = require("../constants");

const extensionTargetDetails = {
  // actor ID for this extention.
  actor: PropTypes.string.isRequired,
  location: PropTypes.string.isRequired,
  // manifestURL points to the manifest.json file. This URL is only valid when debugging
  // local extensions so it might be null.
  manifestURL: PropTypes.string,
  // error message forwarded from the addon manager during reloading temporary extension.
  reloadError: PropTypes.string,
  // unique extension id.
  uuid: PropTypes.string.isRequired,
  // warning messages forwarded from the addon manager.
  warnings: PropTypes.arrayOf(PropTypes.string).isRequired,
};

const processTargetDetails = {
  // Description for the process.
  description: PropTypes.string.isRequired,
  // The id for the process. #0 is the main/parent process, #1++ are parent processes
  processId: PropTypes.number.isRequired,
};

const tabTargetDetails = {
  // the url of the tab.
  url: PropTypes.string.isRequired,
};

const workerTargetDetails = {
  // (service worker specific) one of "LISTENING", "NOT_LISTENING". undefined otherwise.
  fetch: PropTypes.string,
  // front for the ServiceWorkerRegistration related to this service worker.
  registrationFront: PropTypes.object,
  // (service worker specific) scope of the service worker registration.
  scope: PropTypes.string,
  // (service worker specific) one of "RUNNING", "REGISTERING", "STOPPED".
  status: PropTypes.string,
};

const debugTarget = {
  // details property will contain a type-specific object.
  details: PropTypes.oneOfType([
    PropTypes.shape(extensionTargetDetails),
    PropTypes.shape(processTargetDetails),
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
  // one of "extension", "tab", "worker", "process".
  type: PropTypes.oneOf(Object.values(DEBUG_TARGETS)).isRequired,
};

exports.debugTarget = PropTypes.shape(debugTarget);
