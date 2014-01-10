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

  function moveMouseOver(aElement, aInspector, cb)
  {
    EventUtils.synthesizeMouse(aElement, 2, 2, {type: "mousemove"},
      aElement.ownerDocument.defaultView);
    aInspector.toolbox.once("picker-node-hovered", () => {
      executeSoon(cb);
    });
  }

  function isHighlighting()
  {
    let outline = gBrowser.selectedBrowser.parentNode
      .querySelector(".highlighter-container .highlighter-outline");
    return outline && !outline.hasAttribute("hidden");
  }

  function inspectorShouldBeOpenAndHighlighting(aInspector, aToolbox)
  {
    is (aToolbox.currentToolId, "inspector", "Correct tool has been loaded");

    aToolbox.once("picker-started", () => {
      ok(true, "picker-started event received, highlighter started");
      keysetMap.inspector.synthesizeKey();

      aToolbox.once("picker-stopped", () => {
        ok(true, "picker-stopped event received, highlighter stopped");
        aToolbox.once("webconsole-ready", (e, panel) => {
          webconsoleShouldBeSelected(aToolbox, panel);
        });
        keysetMap.webconsole.synthesizeKey();
      });
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
