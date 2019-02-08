/* eslint-env webextensions */

"use strict";

browser.runtime.onUpdateAvailable.addListener(details => {
  // By listening to but ignoring this event, any updates will
  // be delayed until the next browser restart.
  // Note that if we ever wanted to change this, we should make
  // sure we manually invalidate the startup cache using the
  // startupcache-invalidate notification.
});
