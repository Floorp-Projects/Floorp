/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/browser/devtools/framework/test/shared-head.js", this);
Services.scriptloader.loadSubScript(TEST_DIR + "../../../commandline/test/helpers.js", this);

const { Eyedropper, EyedropperManager } = require("devtools/client/eyedropper/eyedropper");

function waitForClipboard(setup, expected) {
  let deferred = promise.defer();
  SimpleTest.waitForClipboard(expected, setup, deferred.resolve, deferred.reject);
  return deferred.promise;
}

function dropperStarted(dropper) {
  if (dropper.isStarted) {
    return promise.resolve();
  }
  return dropper.once("started");
}

function dropperLoaded(dropper) {
  if (dropper.loaded) {
    return promise.resolve();
  }
  return dropper.once("load");
}
