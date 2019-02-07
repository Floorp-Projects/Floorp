/* eslint-env webextensions */

"use strict";

browser.runtime.onUpdateAvailable.addListener(details => {
  // By listening to but ignoring this event, any updates will
  // be delayed until the next browser restart.
});
