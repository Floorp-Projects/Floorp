/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-worker */

"use strict";

// Order of these are important.
/* import-globals-from /toolkit/components/workerloader/require.js */
/* import-globals-from Tokenize.jsm */
/* import-globals-from NaiveBayesTextTagger.jsm */
/* import-globals-from NmfTextTagger.jsm */
/* import-globals-from RecipeExecutor.jsm */
/* import-globals-from PersonalityProviderWorkerClass.jsm */
importScripts(
  "resource://gre/modules/workers/require.js",
  "resource://activity-stream/lib/PersonalityProvider/Tokenize.jsm",
  "resource://activity-stream/lib/PersonalityProvider/NaiveBayesTextTagger.jsm",
  "resource://activity-stream/lib/PersonalityProvider/NmfTextTagger.jsm",
  "resource://activity-stream/lib/PersonalityProvider/RecipeExecutor.jsm",
  "resource://activity-stream/lib/PersonalityProvider/PersonalityProviderWorkerClass.jsm"
);

const PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

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
