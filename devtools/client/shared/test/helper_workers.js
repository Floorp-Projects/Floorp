/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

/* import-globals-from ../../debugger/test/mochitest/helpers.js */
/* import-globals-from ../../debugger/test/mochitest/helpers/context.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/helpers.js",
  this
);

var { DevToolsServer } = require("devtools/server/devtools-server");
var { DevToolsClient } = require("devtools/client/devtools-client");
var { Toolbox } = require("devtools/client/framework/toolbox");
loader.lazyRequireGetter(this, "defer", "devtools/shared/defer");

const FRAME_SCRIPT_URL = getRootDirectory(gTestPath) + "code_frame-script.js";

var nextId = 0;

function jsonrpc(tab, method, params) {
  return new Promise(function(resolve, reject) {
    const currentId = nextId++;
    const messageManager = tab.linkedBrowser.messageManager;
    messageManager.sendAsyncMessage("jsonrpc", {
      method: method,
      params: params,
      id: currentId,
    });
    messageManager.addMessageListener("jsonrpc", function listener(res) {
      const {
        data: { result, error, id },
      } = res;
      if (id !== currentId) {
        return;
      }

      messageManager.removeMessageListener("jsonrpc", listener);
      if (error != null) {
        reject(error);
      }

      resolve(result);
    });
  });
}

function createWorkerInTab(tab, url) {
  info("Creating worker with url '" + url + "' in tab.");

  return jsonrpc(tab, "createWorker", [url]);
}

function terminateWorkerInTab(tab, url) {
  info("Terminating worker with url '" + url + "' in tab.");

  return jsonrpc(tab, "terminateWorker", [url]);
}

function postMessageToWorkerInTab(tab, url, message) {
  info("Posting message to worker with url '" + url + "' in tab.");

  return jsonrpc(tab, "postMessageToWorker", [url, message]);
}

function generateMouseClickInTab(tab, path) {
  info("Generating mouse click in tab.");

  return jsonrpc(tab, "generateMouseClick", [path]);
}

function evalInTab(tab, string) {
  info("Evalling string in tab.");

  return jsonrpc(tab, "_eval", [string]);
}

function callInTab(tab, name) {
  info("Calling function with name '" + name + "' in tab.");

  return jsonrpc(tab, "call", [name, Array.prototype.slice.call(arguments, 2)]);
}

function connect(client) {
  info("Connecting client.");
  return client.connect();
}

function close(client) {
  info("Waiting for client to close.\n");
  return client.close();
}

function listTabs(client) {
  info("Listing tabs.");
  return client.mainRoot.listTabs();
}

function findTab(tabs, url) {
  info("Finding tab with url '" + url + "'.");
  for (const tab of tabs) {
    if (tab.url === url) {
      return tab;
    }
  }
  return null;
}

function listWorkers(targetFront) {
  info("Listing workers.");
  return targetFront.listWorkers();
}

function findWorker(workers, url) {
  info("Finding worker with url '" + url + "'.");
  for (const worker of workers) {
    if (worker.url === url) {
      return worker;
    }
  }
  return null;
}

function waitForWorkerListChanged(targetFront) {
  info("Waiting for worker list to change.");
  return targetFront.once("workerListChanged");
}

function attachThread(workerTargetFront, options) {
  info("Attaching to thread.");
  return workerTargetFront.attachThread(options);
}

async function waitForWorkerClose(workerTargetFront) {
  info("Waiting for worker to close.");
  await workerTargetFront.once("close");
  info("Worker did close.");
}

// Return a promise with a reference to webconsole, opening the split
// console if necessary.  This cleans up the split console pref so
// it won't pollute other tests.
async function getSplitConsole(toolbox, win) {
  if (!win) {
    win = toolbox.win;
  }

  if (!toolbox.splitConsole) {
    EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  }

  await toolbox.getPanelWhenReady("webconsole");
  ok(toolbox.splitConsole, "Split console is shown.");
  return toolbox.getPanel("webconsole");
}

function executeAndWaitForMessage(
  webconsole,
  expression,
  expectedTextContent,
  className = "result"
) {
  const { ui } = webconsole.hud;
  return new Promise(resolve => {
    const onNewMessages = messages => {
      for (const message of messages) {
        if (
          message.node.classList.contains(className) &&
          message.node.textContent.includes(expectedTextContent)
        ) {
          ui.off("new-messages", onNewMessages);
          resolve(message.node);
        }
      }
    };
    ui.on("new-messages", onNewMessages);
    ui.wrapper.dispatchEvaluateExpression(expression);
  });
}

async function initWorkerDebugger(TAB_URL, WORKER_URL) {
  const tab = await addTab(TAB_URL);
  const target = await TargetFactory.forTab(tab);
  await target.attach();
  const { client } = target;

  await createWorkerInTab(tab, WORKER_URL);

  const { workers } = await listWorkers(target);
  const workerTargetFront = findWorker(workers, WORKER_URL);

  const toolbox = await gDevTools.showToolbox(
    workerTargetFront,
    "jsdebugger",
    Toolbox.HostType.WINDOW
  );

  const debuggerPanel = toolbox.getCurrentPanel();

  const gDebugger = debuggerPanel.panelWin;

  const context = createDebuggerContext(toolbox);

  return {
    ...context,
    client,
    tab,
    target,
    workerTargetFront,
    toolbox,
    gDebugger,
  };
}

// Override addTab/removeTab as defined by shared-head, since these have
// an extra window parameter and add a frame script
this.addTab = function addTab(url, win) {
  info("Adding tab: " + url);

  const deferred = defer();
  const targetWindow = win || window;
  const targetBrowser = targetWindow.gBrowser;

  targetWindow.focus();
  const tab = (targetBrowser.selectedTab = BrowserTestUtils.addTab(
    targetBrowser,
    url
  ));
  const linkedBrowser = tab.linkedBrowser;

  info("Loading frame script with url " + FRAME_SCRIPT_URL + ".");
  linkedBrowser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);

  BrowserTestUtils.browserLoaded(linkedBrowser).then(function() {
    info("Tab added and finished loading: " + url);
    deferred.resolve(tab);
  });

  return deferred.promise;
};

this.removeTab = function removeTab(tab, win) {
  info("Removing tab.");

  const deferred = defer();
  const targetWindow = win || window;
  const targetBrowser = targetWindow.gBrowser;
  const tabContainer = targetBrowser.tabContainer;

  tabContainer.addEventListener(
    "TabClose",
    function() {
      info("Tab removed and finished closing.");
      deferred.resolve();
    },
    { once: true }
  );

  targetBrowser.removeTab(tab);
  return deferred.promise;
};

async function attachThreadActorForTab(tab) {
  const target = await TargetFactory.forTab(tab);
  await target.attach();
  const threadFront = await target.attachThread();
  await threadFront.resume();
  return { client: target.client, threadFront };
}

function pushPrefs(...aPrefs) {
  const deferred = defer();
  SpecialPowers.pushPrefEnv({ set: aPrefs }, deferred.resolve);
  return deferred.promise;
}

function popPrefs() {
  const deferred = defer();
  SpecialPowers.popPrefEnv(deferred.resolve);
  return deferred.promise;
}
