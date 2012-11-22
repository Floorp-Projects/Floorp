/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let temp = [];
Cu.import("resource:///modules/devtools/Target.jsm", temp);
let TargetFactory = temp.TargetFactory;

function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    openAllTools();
  }, true);

  function openAllTools() {
    let target = TargetFactory.forTab(gBrowser.selectedTab);

    let tools = gDevTools.getToolDefinitions();
    let expectedCallbacksCount = tools.size;

    let firstTool = null;
    // we transform the map to a [id, eventHasBeenFiredYet] map
    for (let [id] of tools) {
      if (!firstTool)
        firstTool = id;
      tools.set(id, false);
    }

    let toolbox = gDevTools.openToolbox(target, undefined, firstTool);

    // We add the event listeners
    for (let [toolId] of tools) {
      let id = toolId;
      info("Registering listener for " + id);
      tools.set(id, false);
      toolbox.on(id + "-ready", function(event, panel) {
        expectedCallbacksCount--;
        info("Got event "  + event);
        is(toolbox.getToolPanels().get(id), panel, "Got the right tool panel for " + id);
        tools.set(id, true);
        if (expectedCallbacksCount == 0) {
          // "executeSoon" because we want to let a chance
          // to falsy code to fire unexpected ready events.
          executeSoon(theEnd);
        }
        if (expectedCallbacksCount < 0) {
          ok(false, "we are receiving too many events");
        }
      });
    }

    toolbox.once("ready", function() {
      // We open all the 
      for (let [id] of tools) {
        if (id != firstTool) {
          toolbox.selectTool(id);
        }
      }
    });

    function theEnd() {
      for (let [id, called] of tools) {
        ok(called, "Tool " + id + " has fired its ready event");
      }
      toolbox.destroy();
      gBrowser.removeCurrentTab();
      finish();
    }
  }
}
