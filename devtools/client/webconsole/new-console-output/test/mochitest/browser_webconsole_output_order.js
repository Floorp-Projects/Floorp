/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that any output created from calls to the console API comes before the
// echoed JavaScript.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "new-console-output/test/mochitest/test-console.html";

add_task(async function () {
  let hud = await openNewTabAndConsole(TEST_URI);
  hud.jsterm.clearOutput();

  let messages = ["console.log('foo', 'bar');", "foo bar", "undefined"];
  let onMessages = waitForMessages({
    hud,
    messages: messages.map(text => ({text}))
  });

  hud.jsterm.execute("console.log('foo', 'bar');");

  const [fncallNode, consoleMessageNode, resultNode] =
    (await onMessages).map(msg => msg.node);

  is(fncallNode.nextElementSibling, consoleMessageNode,
     "console.log() is followed by 'foo' 'bar'");
  is(consoleMessageNode.nextElementSibling, resultNode,
     "'foo' 'bar' is followed by undefined");
});
