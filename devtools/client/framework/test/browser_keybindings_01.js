/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the keybindings for opening and closing the inspector work as expected
// Can probably make this a shared test that tests all of the tools global keybindings
const TEST_URL = "data:text/html,<html><head><title>Test for the " +
                 "highlighter keybindings</title></head><body>" +
                 "<h1>Keybindings!</h1></body></html>"

// Use the new debugger frontend because the old one swallows the netmonitor shortcut:
// https://bugzilla.mozilla.org/show_bug.cgi?id=1370442#c7
Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", true);
registerCleanupFunction(function* () {
  Services.prefs.clearUserPref("devtools.debugger.new-debugger-frontend");
});

const {gDevToolsBrowser} = require("devtools/client/framework/devtools-browser");

function test()
{
  waitForExplicitFinish();

  let doc;
  let node;
  let inspector;
  let keysetMap = { };

  addTab(TEST_URL).then(function () {
    doc = content.document;
    node = doc.querySelector("h1");
    waitForFocus(setupKeyBindingsTest);
  });

  function buildDevtoolsKeysetMap(keyset) {
    [].forEach.call(keyset.querySelectorAll("key"), function (key) {

      if (!key.getAttribute("key")) {
        return;
      }

      let modifiers = key.getAttribute("modifiers");

      keysetMap[key.id.split("_")[1]] = {
        key: key.getAttribute("key"),
        modifiers: modifiers,
        modifierOpt: {
          shiftKey: modifiers.match("shift"),
          ctrlKey: modifiers.match("ctrl"),
          altKey: modifiers.match("alt"),
          metaKey: modifiers.match("meta"),
          accelKey: modifiers.match("accel")
        },
        synthesizeKey: function () {
          EventUtils.synthesizeKey(this.key, this.modifierOpt);
        }
      };
    });
  }

  function setupKeyBindingsTest()
  {
    for (let win of gDevToolsBrowser._trackedBrowserWindows) {
      buildDevtoolsKeysetMap(win.document.getElementById("devtoolsKeyset"));
    }

    gDevTools.once("toolbox-ready", (e, toolbox) => {
      inspectorShouldBeOpenAndHighlighting(toolbox.getCurrentPanel(), toolbox);
    });

    keysetMap.inspector.synthesizeKey();
  }

  function inspectorShouldBeOpenAndHighlighting(aInspector, aToolbox)
  {
    is(aToolbox.currentToolId, "inspector", "Correct tool has been loaded");

    aToolbox.once("picker-started", () => {
      ok(true, "picker-started event received, highlighter started");
      keysetMap.inspector.synthesizeKey();

      aToolbox.once("picker-stopped", () => {
        ok(true, "picker-stopped event received, highlighter stopped");
        gDevTools.once("select-tool-command", () => {
          webconsoleShouldBeSelected(aToolbox);
        });
        keysetMap.webconsole.synthesizeKey();
      });
    });
  }

  function webconsoleShouldBeSelected(aToolbox)
  {
    is(aToolbox.currentToolId, "webconsole", "webconsole should be selected.");

    gDevTools.once("select-tool-command", () => {
      jsdebuggerShouldBeSelected(aToolbox);
    });
    keysetMap.jsdebugger.synthesizeKey();
  }

  function jsdebuggerShouldBeSelected(aToolbox)
  {
    is(aToolbox.currentToolId, "jsdebugger", "jsdebugger should be selected.");

    gDevTools.once("select-tool-command", () => {
      netmonitorShouldBeSelected(aToolbox);
    });

    keysetMap.netmonitor.synthesizeKey();
  }

  function netmonitorShouldBeSelected(aToolbox, panel)
  {
    is(aToolbox.currentToolId, "netmonitor", "netmonitor should be selected.");
    finishUp();
  }

  function finishUp() {
    doc = node = inspector = keysetMap = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
