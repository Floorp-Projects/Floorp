/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function testNotActivated() {
  // dom.performance.enable_scheduler_timing is set to false in browser.ini
  waitForExplicitFinish();
  // make sure we throw a JS exception in case the pref is off and
  // we call requestPerformanceMetrics()
  let failed = false;
  try {
    await ChromeUtils.requestPerformanceMetrics();
  } catch (e) {
    failed = true;
  }
  Assert.ok(failed, "We should get an exception if preffed off");
});
