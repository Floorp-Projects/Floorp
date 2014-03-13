/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["LanguageDetector"];

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");

const WORKER_URL = "resource:///modules/translation/cld-worker.js";

let detectionQueue = [];

let workerReady = false;
let pendingStrings = [];

XPCOMUtils.defineLazyGetter(this, "worker", () => {
  let worker = new Worker(WORKER_URL);
  worker.onmessage = function(aMsg) {
    if (aMsg.data == "ready") {
      workerReady = true;
      for (let string of pendingStrings)
        worker.postMessage(string);
      pendingStrings = [];
    }
    else
      detectionQueue.shift().resolve(aMsg.data);
  }
  return worker;
});

this.LanguageDetector = {
  /**
   * Detect the language of a given string
   *
   * @returns {Promise}
   * @resolves When detection is finished, with a object containing
   * these fields:
   *  - 'language' (string with a language code)
   *  - 'confident' (boolean).
   */
  detectLanguage: function(aString) {
    let deferred = Promise.defer();
    detectionQueue.push(deferred);
    if (worker && workerReady)
      worker.postMessage(aString);
    else
      pendingStrings.push(aString);
    return deferred.promise;
  }
};
