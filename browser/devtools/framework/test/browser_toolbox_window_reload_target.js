/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "data:text/html;charset=utf-8,"+
                 "<html><head><title>Test reload</title></head>"+
                 "<body><h1>Testing reload from devtools</h1></body></html>";

var {Toolbox} = require("devtools/framework/toolbox");

var target, toolbox, description, reloadsSent, toolIDs;

function test() {
  addTab(TEST_URL).then(() => {
    target = TargetFactory.forTab(gBrowser.selectedTab);

    target.makeRemote().then(() => {
      toolIDs = gDevTools.getToolDefinitionArray()
                  .filter(def => def.isTargetSupported(target))
                  .map(def => def.id);
      gDevTools.showToolbox(target, toolIDs[0], Toolbox.HostType.BOTTOM)
               .then(startReloadTest);
    });
  });
}

function startReloadTest(aToolbox) {
  getFrameScript(); // causes frame-script-utils to be loaded into the child.
  toolbox = aToolbox;

  reloadsSent = 0;
  let reloads = 0;
  let reloadCounter = (msg) => {
    reloads++;
    info("Detected reload #"+reloads);
    is(reloads, reloadsSent, "Reloaded from devtools window once and only for "+description+"");
  };
  gBrowser.selectedBrowser.messageManager.addMessageListener("devtools:test:load", reloadCounter);

  testAllTheTools("docked", () => {
    let origHostType = toolbox.hostType;
    toolbox.switchHost(Toolbox.HostType.WINDOW).then(() => {
      toolbox.doc.defaultView.focus();
      testAllTheTools("undocked", () => {
        toolbox.switchHost(origHostType).then(() => {
          gBrowser.selectedBrowser.messageManager.removeMessageListener("devtools:test:load", reloadCounter);
          // If we finish too early, the inspector breaks promises:
          toolbox.getPanel("inspector").once("new-root", finishUp);
        });
      });
    });
  }, toolIDs.length-1 /* only test 1 tool in docked mode, to cut down test time */);
}

function testAllTheTools(docked, callback, toolNum=0) {
  if (toolNum >= toolIDs.length) {
    return callback();
  }
  toolbox.selectTool(toolIDs[toolNum]).then(() => {
    testReload("toolbox-reload-key", docked, toolIDs[toolNum], () => {
      testReload("toolbox-reload-key2", docked, toolIDs[toolNum], () => {
        testReload("toolbox-force-reload-key", docked, toolIDs[toolNum], () => {
          testReload("toolbox-force-reload-key2", docked, toolIDs[toolNum], () => {
            testAllTheTools(docked, callback, toolNum+1);
          });
        });
      });
    });
  });
}

function testReload(key, docked, toolID, callback) {
  let complete = () => {
    gBrowser.selectedBrowser.messageManager.removeMessageListener("devtools:test:load", complete);
    return callback();
  };
  gBrowser.selectedBrowser.messageManager.addMessageListener("devtools:test:load", complete);

  description = docked+" devtools with tool "+toolID+", key #" + key;
  info("Testing reload in "+description);
  let el = toolbox.doc.getElementById(key);
  synthesizeKeyElement(el);
  reloadsSent++;
}

function finishUp() {
  toolbox.destroy().then(() => {
    gBrowser.removeCurrentTab();

    target = toolbox = description = reloadsSent = toolIDs = null;

    finish();
  });
}
