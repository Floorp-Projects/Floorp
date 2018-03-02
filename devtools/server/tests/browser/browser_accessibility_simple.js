/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Simple checks for the AccessibilityActor and AccessibleWalkerActor

add_task(async function () {
  let {client, accessibility} = await initAccessibilityFrontForUrl(
    "data:text/html;charset=utf-8,<title>test</title><div></div>");

  ok(accessibility, "The AccessibilityFront was created");
  ok(accessibility.getWalker, "The getWalker method exists");

  let a11yWalker = await accessibility.getWalker();
  ok(a11yWalker, "The AccessibleWalkerFront was returned");

  await client.close();
  gBrowser.removeCurrentTab();
});
