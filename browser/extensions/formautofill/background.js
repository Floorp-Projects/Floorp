/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env webextensions */

"use strict";

browser.runtime.onUpdateAvailable.addListener(_details => {
  // By listening to but ignoring this event, any updates will
  // be delayed until the next browser restart.
  // Note that if we ever wanted to change this, we should make
  // sure we manually invalidate the startup cache using the
  // startupcache-invalidate notification.
});
