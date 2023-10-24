/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PersonalityProviderWorker } from "resource://activity-stream/lib/PersonalityProvider/PersonalityProviderWorkerClass.mjs";

import { PromiseWorker } from "resource://gre/modules/workers/PromiseWorker.mjs";

const personalityProviderWorker = new PersonalityProviderWorker();

// This is boiler plate worker stuff that connects it to the main thread PromiseWorker.
const worker = new PromiseWorker.AbstractWorker();
worker.dispatch = function (method, args = []) {
  return personalityProviderWorker[method](...args);
};
worker.postMessage = function (message, ...transfers) {
  self.postMessage(message, ...transfers);
};
worker.close = function () {
  self.close();
};

self.addEventListener("message", msg => worker.handleMessage(msg));
self.addEventListener("unhandledrejection", function (error) {
  throw error.reason;
});
