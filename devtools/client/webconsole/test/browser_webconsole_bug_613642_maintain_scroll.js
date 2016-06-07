/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var TEST_URI = "data:text/html;charset=utf-8,Web Console test for " +
               "bug 613642: remember scroll location";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  hud.jsterm.clearOutput();
  let outputNode = hud.outputNode;
  let scrollBox = hud.ui.outputWrapper;

  for (let i = 0; i < 150; i++) {
    ContentTask.spawn(gBrowser.selectedBrowser, i, function* (num) {
      content.console.log("test message " + num);
    });
  }

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test message 149",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  ok(scrollBox.scrollTop > 0, "scroll location is not at the top");

  // scroll to the first node
  outputNode.focus();

  let scrolled = promise.defer();

  scrollBox.onscroll = () => {
    info("onscroll top " + scrollBox.scrollTop);
    if (scrollBox.scrollTop != 0) {
      // Wait for scroll to 0.
      return;
    }
    scrollBox.onscroll = null;
    is(scrollBox.scrollTop, 0, "scroll location updated (moved to top)");
    scrolled.resolve();
  };
  EventUtils.synthesizeKey("VK_HOME", {}, hud.iframeWindow);

  yield scrolled.promise;

  // add a message and make sure scroll doesn't change
  ContentTask.spawn(gBrowser.selectedBrowser, null,
    "() => content.console.log('test message 150')");

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test message 150",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  scrolled = promise.defer();
  scrollBox.onscroll = () => {
    if (scrollBox.scrollTop != 0) {
      // Wait for scroll to stabilize at the top.
      return;
    }
    scrollBox.onscroll = null;
    is(scrollBox.scrollTop, 0, "scroll location is still at the top");
    scrolled.resolve();
  };

  // Make sure that scroll stabilizes at the top. executeSoon() is needed for
  // the yield to work.
  executeSoon(scrollBox.onscroll);

  yield scrolled.promise;

  // scroll back to the bottom
  outputNode.lastChild.focus();

  scrolled = promise.defer();
  scrollBox.onscroll = () => {
    if (scrollBox.scrollTop == 0) {
      // Wait for scroll to bottom.
      return;
    }
    scrollBox.onscroll = null;
    isnot(scrollBox.scrollTop, 0, "scroll location updated (moved to bottom)");
    scrolled.resolve();
  };
  EventUtils.synthesizeKey("VK_END", {});
  yield scrolled.promise;

  let oldScrollTop = scrollBox.scrollTop;

  ContentTask.spawn(gBrowser.selectedBrowser, null,
    "() => content.console.log('test message 151')");

  scrolled = promise.defer();
  scrollBox.onscroll = () => {
    if (scrollBox.scrollTop == oldScrollTop) {
      // Wait for scroll to change.
      return;
    }
    scrollBox.onscroll = null;
    isnot(scrollBox.scrollTop, oldScrollTop,
          "scroll location updated (moved to bottom again)");
    scrolled.resolve();
  };
  yield scrolled.promise;
});
