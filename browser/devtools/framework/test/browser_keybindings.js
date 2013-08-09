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
    waitForFocus(setupKeyBindingsTest, content);
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
      inspectorShouldBeOpenAndHighlighting(toolbox.getCurrentPanel(), toolbox)
    });

    keysetMap.inspector.synthesizeKey();
  }

  function inspectorShouldBeOpenAndHighlighting(aInspector, aToolbox)
  {
    is (aToolbox.currentToolId, "inspector", "Correct tool has been loaded");
    is (aInspector.highlighter.locked, true, "Highlighter should be locked");

    aInspector.highlighter.once("unlocked", () => {
      is (aInspector.highlighter.locked, false, "Highlighter should be unlocked");
      keysetMap.inspector.synthesizeKey();
      is (aInspector.highlighter.locked, true, "Highlighter should be locked");
      keysetMap.inspector.synthesizeKey();
      is (aInspector.highlighter.locked, false, "Highlighter should be unlocked");
      keysetMap.inspector.synthesizeKey();
      is (aInspector.highlighter.locked, true, "Highlighter should be locked");

      aToolbox.once("webconsole-ready", (e, panel) => {
        webconsoleShouldBeSelected(aToolbox, panel);
      });
      keysetMap.webconsole.synthesizeKey();
    });
  }

  function webconsoleShouldBeSelected(aToolbox, panel)
  {
      is (aToolbox.currentToolId, "webconsole");

      aToolbox.once("jsdebugger-ready", (e, panel) => {
        jsdebuggerShouldBeSelected(aToolbox, panel);
      });
      keysetMap.jsdebugger.synthesizeKey();
  }

  function jsdebuggerShouldBeSelected(aToolbox, panel)
  {
      is (aToolbox.currentToolId, "jsdebugger");

      aToolbox.once("netmonitor-ready", (e, panel) => {
        netmonitorShouldBeSelected(aToolbox, panel);
      });

      keysetMap.netmonitor.synthesizeKey();
  }

  function netmonitorShouldBeSelected(aToolbox, panel)
  {
      is (aToolbox.currentToolId, "netmonitor");
      finishUp();
  }

  function finishUp() {
    doc = node = inspector = keysetMap = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
