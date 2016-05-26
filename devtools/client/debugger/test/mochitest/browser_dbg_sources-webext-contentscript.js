/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure eval scripts appear in the source list
 */

const TAB_URL = EXAMPLE_URL + "doc_script_webext_contentscript.html";

function test() {
  let gPanel, gDebugger;
  let gSources, gAddon;

  let cleanup = function* (e) {
    if (gAddon) {
      // Remove the addon, if any.
      yield removeAddon(gAddon);
    }
    if (gPanel) {
      // Close the debugger panel, if any.
      yield closeDebuggerAndFinish(gPanel);
    } else {
      // If no debugger panel was opened, call finish directly.
      finish();
    }
  };

  return Task.spawn(function* () {
    gAddon = yield addAddon(EXAMPLE_URL + "/addon-webext-contentscript.xpi");

    let options = {
      source: "webext-content-script.js",
      line: 1
    };
    [,, gPanel] = yield initDebugger(TAB_URL, options);
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    is(gSources.values.length, 1, "Should have 1 source");

    let item = gSources.getItemForAttachment(attachment => {
      return attachment.source.url.includes("moz-extension");
    });

    ok(item, "Got the expected WebExtensions ContentScript source");
    ok(item && item.attachment.source.url.includes(item.attachment.group),
       "The source is in the expected source group");
    is(item && item.attachment.label, "webext-content-script.js",
       "Got the expected filename in the label");

    yield cleanup();
  }).catch((e) => {
    ok(false, `Got an unexpected exception: ${e}`);
    // Cleanup in case of failures in the above task.
    return Task.spawn(cleanup);
  });
}
