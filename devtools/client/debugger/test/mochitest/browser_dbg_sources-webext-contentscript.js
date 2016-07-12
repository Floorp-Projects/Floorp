/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure eval scripts appear in the source list
 */

const ADDON_PATH = "addon-webext-contentscript.xpi";
const TAB_URL = EXAMPLE_URL + "doc_script_webext_contentscript.html";

let {getExtensionUUID} = Cu.import("resource://gre/modules/Extension.jsm", {});

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
    gAddon = yield addTemporaryAddon(ADDON_PATH);
    let uuid = getExtensionUUID(gAddon.id);

    let options = {
      source: `moz-extension://${uuid}/webext-content-script.js`,
      line: 1
    };
    [,, gPanel] = yield initDebugger(TAB_URL, options);
    gDebugger = gPanel.panelWin;
    gSources = gDebugger.DebuggerView.Sources;

    is(gSources.values.length, 2, "Should have 2 sources");

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
