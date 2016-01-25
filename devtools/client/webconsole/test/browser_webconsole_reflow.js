/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
                 "reflow activity";

add_task(function* () {
  let { browser } = yield loadTab(TEST_URI);

  let hud = yield openConsole();

  function onReflowListenersReady() {
    browser.contentDocument.body.style.display = "none";
    browser.contentDocument.body.clientTop;
  }

  Services.prefs.setBoolPref("devtools.webconsole.filter.csslog", true);
  hud.ui._updateReflowActivityListener(onReflowListenersReady);
  Services.prefs.clearUserPref("devtools.webconsole.filter.csslog");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: /reflow: /,
      category: CATEGORY_CSS,
      severity: SEVERITY_LOG,
    }],
  });
});
