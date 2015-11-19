/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the keybindings for opening and closing the inspector work as expected
// Can probably make this a shared test that tests all of the tools global keybindings

function test()
{
  waitForExplicitFinish();

  let doc;
  let node;
  let inspector;
  let keysetMap = { };

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    node = doc.querySelector("h1");
    waitForFocus(setupKeyBindingsTest);
  }, true);

  content.location = "data:text/html,<html><head><title>Test for the " +
                     "highlighter keybindings</title></head><body>" +
                     "<h1>Keybindings!</h1></body></html>";

  function buildDevtoolsKeysetMap(keyset) {
    [].forEach.call(keyset.querySelectorAll("key"), function(key) {

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
        synthesizeKey: function() {
          EventUtils.synthesizeKey(this.key, this.modifierOpt);
        }
      }
    });
  }

  function setupKeyBindingsTest()
  {
    for (let win of gDevToolsBrowser._trackedBrowserWindows) {
      buildDevtoolsKeysetMap(win.document.getElementById("devtoolsKeyset"));
    }

    gDevTools.once("toolbox-ready", (e, toolbox) => {
      // The inspector is already selected at this point, but we wait
      // for the signal here to avoid confusing ourselves when we wait
      // for the next select-tool-command signal.
      gDevTools.once("select-tool-command", () => {
        inspectorShouldBeSelected(toolbox.getCurrentPanel(), toolbox);
      });
    });

    keysetMap.inspector.synthesizeKey();
  }

  function inspectorShouldBeSelected(aInspector, aToolbox)
  {
    is (aToolbox.currentToolId, "inspector", "Correct tool has been loaded");

    gDevTools.once("select-tool-command", (x, toolId) => {
      webconsoleShouldBeSelected(aToolbox);
    });
    keysetMap.webconsole.synthesizeKey();
  }

  function webconsoleShouldBeSelected(aToolbox)
  {
      is (aToolbox.currentToolId, "webconsole", "webconsole should be selected.");

      gDevTools.once("select-tool-command", () => {
        jsdebuggerShouldBeSelected(aToolbox);
      });
      keysetMap.jsdebugger.synthesizeKey();
  }

  function jsdebuggerShouldBeSelected(aToolbox)
  {
      is (aToolbox.currentToolId, "jsdebugger", "jsdebugger should be selected.");

      gDevTools.once("select-tool-command", () => {
        netmonitorShouldBeSelected(aToolbox);
      });

      keysetMap.netmonitor.synthesizeKey();
  }

  function netmonitorShouldBeSelected(aToolbox, panel)
  {
      is (aToolbox.currentToolId, "netmonitor", "netmonitor should be selected.");
      finishUp();
  }

  function finishUp() {
    doc = node = inspector = keysetMap = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
