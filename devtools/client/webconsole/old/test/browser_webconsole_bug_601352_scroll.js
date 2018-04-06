/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the console output scrolls to JS eval results when there are many
// messages displayed. See bug 601352.

"use strict";

add_task(function* () {
  let {tab} = yield loadTab("data:text/html;charset=utf-8,Web Console test " +
                            "for bug 601352");
  let hud = yield openConsole(tab);
  hud.jsterm.clearOutput();

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let longMessage = "";
    for (let i = 0; i < 50; i++) {
      longMessage += "LongNonwrappingMessage";
    }

    for (let i = 0; i < 50; i++) {
      content.console.log("test1 message " + i);
    }

    content.console.log(longMessage);

    for (let i = 0; i < 50; i++) {
      content.console.log("test2 message " + i);
    }
  });

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "test1 message 0",
    }, {
      text: "test1 message 49",
    }, {
      text: "LongNonwrappingMessage",
    }, {
      text: "test2 message 0",
    }, {
      text: "test2 message 49",
    }],
  });

  let node = yield hud.jsterm.execute("1+1");

  let scrollNode = hud.ui.outputWrapper;
  let rectNode = node.getBoundingClientRect();
  let rectOutput = scrollNode.getBoundingClientRect();

  yield ContentTask.spawn(gBrowser.selectedBrowser, {
    rectNode,
    rectOutput,
    scrollHeight: scrollNode.scrollHeight,
    scrollTop: scrollNode.scrollTop,
    clientHeight: scrollNode.clientHeight,
  }, function* (args) {
    console.debug("rectNode", args.rectNode, "rectOutput", args.rectOutput);
    console.log("scrollNode scrollHeight", args.scrollHeight,
                "scrollTop", args.scrollTop, "clientHeight",
                args.clientHeight);
  });

  isnot(scrollNode.scrollTop, 0, "scroll location is not at the top");

  // The bounding client rect .top/left coordinates are relative to the
  // console iframe.

  // Visible scroll viewport.
  let height = rectOutput.height;

  // Top and bottom coordinates of the last message node, relative to the
  // outputNode.
  let top = rectNode.top - rectOutput.top;
  let bottom = top + rectNode.height;
  info("node top " + top + " node bottom " + bottom + " node clientHeight " +
       node.clientHeight);

  ok(top >= 0 && bottom <= height, "last message is visible");
});
