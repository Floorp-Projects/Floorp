/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let Toolbox = devtools.Toolbox;

let toolbox, toolIDs, idIndex, secondTime = false,
    reverse = false, nextKey = null, prevKey = null;

function test() {
  waitForExplicitFinish();

  addTab("about:blank", function() {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    idIndex = 0;

    target.makeRemote().then(() => {
      toolIDs = gDevTools.getToolDefinitionArray()
                  .filter(def => def.isTargetSupported(target))
                  .map(def => def.id);
      gDevTools.showToolbox(target, toolIDs[0], Toolbox.HostType.BOTTOM)
               .then(testShortcuts);
    });
  });
}

function testShortcuts(aToolbox, aIndex) {
  if (aIndex === undefined) {
    aIndex = 1;
  } else if (aIndex == toolIDs.length) {
    aIndex = 0;
    if (secondTime) {
      secondTime = false;
      reverse = true;
      aIndex = toolIDs.length - 2;
    }
    else {
      secondTime = true;
    }
  }
  else if (aIndex == -1) {
    aIndex = toolIDs.length - 1;
    if (secondTime) {
      tidyUp();
      return;
    }
    secondTime = true;
  }

  toolbox = aToolbox;
  if (!nextKey) {
    nextKey = toolbox.doc.getElementById("toolbox-next-tool-key")
                     .getAttribute("key");
    prevKey = toolbox.doc.getElementById("toolbox-previous-tool-key")
                     .getAttribute("key");
  }
  info("Toolbox fired a `ready` event");

  toolbox.once("select", onSelect);

  let key = (reverse ? prevKey: nextKey);
  let modifiers = {
    accelKey: true
  };
  idIndex = aIndex;
  info("Testing shortcut to switch to tool " + aIndex + ":" + toolIDs[aIndex] +
       " using key " + key);
  EventUtils.synthesizeKey(key, modifiers, toolbox.doc.defaultView);
}

function onSelect(event, id) {
  info("toolbox-select event from " + id);

  is(toolIDs.indexOf(id), idIndex,
     "Correct tool is selected on pressing the shortcut for " + id);
  // Execute soon to reset the stack trace.
  executeSoon(() => {
    testShortcuts(toolbox, idIndex + (reverse ? -1: 1));
  });
}

function tidyUp() {
  toolbox.destroy().then(function() {
    gBrowser.removeCurrentTab();

    toolbox = toolIDs = idIndex = Toolbox = secondTime = reverse = nextKey =
      prevKey = null;
    finish();
  });
}
