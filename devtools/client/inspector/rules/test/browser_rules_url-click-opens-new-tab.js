/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Bug 1326626 - Tests that clicking a background url opens a new tab
// even when the devtools is opened in a separate window.

const TEST_URL = "data:text/html,<style>body{background:url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQImWNgYGD4DwABBAEAfbLI3wAAAABJRU5ErkJggg==) no-repeat}";

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL, "window");
  let view = selectRuleView(inspector);

  yield selectNode("body", inspector);

  let anchor = view.styleDocument.querySelector(".ruleview-propertyvaluecontainer a");
  ok(anchor, "Link exists for style tag node");

  let onTabOpened = waitForTab();
  anchor.click();

  info("Wait for the image to open in a new tab");
  let tab = yield onTabOpened;
  ok(tab, "A new tab opened");

  is(tab.linkedBrowser.currentURI.spec, anchor.href, "The new tab has the expected URL");
});
