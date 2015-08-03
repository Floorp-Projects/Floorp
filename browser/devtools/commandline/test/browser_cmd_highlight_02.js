/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the highlight command actually creates a highlighter

const TEST_PAGE = "data:text/html;charset=utf-8,<div></div>";

function test() {
  return Task.spawn(function*() {
    let options = yield helpers.openTab(TEST_PAGE);
    yield helpers.openToolbar(options);

    info("highlighting the body node");
    yield runCommand("highlight body", options);
    is(getHighlighterNumber(), 1, "The highlighter element exists for body");

    info("highlighting the div node");
    yield runCommand("highlight div", options);
    is(getHighlighterNumber(), 1, "The highlighter element exists for div");

    info("highlighting the body node again, asking to keep the div");
    yield runCommand("highlight body --keep", options);
    is(getHighlighterNumber(), 2, "2 highlighter elements have been created");

    info("unhighlighting all nodes");
    yield runCommand("unhighlight", options);
    is(getHighlighterNumber(), 0, "All highlighters have been removed");

    yield helpers.closeToolbar(options);
    yield helpers.closeTab(options);
  }).then(finish, helpers.handleError);
}

function getHighlighterNumber() {
  // Note that this only works as long as gcli tests aren't run with e10s on.
  // To make this e10s ready, execute this in a content frame script instead.
  return require("devtools/toolkit/gcli/commands/highlight").highlighters.length;
}

function* runCommand(cmd, options) {
  yield helpers.audit(options, [{ setup: cmd, exec: {} }]);
}
