/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

/* import-globals-from ../../shared/test/shared-head.js */
/* import-globals-from ../../debugger/test/mochitest/helpers.js */
/* import-globals-from ../../debugger/test/mochitest/helpers/context.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/helpers.js",
  this
);

var { DevToolsServer } = require("devtools/server/devtools-server");
var { DevToolsClient } = require("devtools/client/devtools-client");
var { Toolbox } = require("devtools/client/framework/toolbox");

function createWorkerInTab(tab, url) {
  info("Creating worker with url '" + url + "' in tab.");

  return SpecialPowers.spawn(tab.linkedBrowser, [url], urlChild => {
    return new Promise(resolve => {
      const worker = new content.Worker(urlChild);
      worker.addEventListener(
        "message",
        function() {
          if (!content.workers) {
            content.workers = [];
          }
          content.workers[urlChild] = worker;
          resolve();
        },
        { once: true }
      );
    });
  });
}

function terminateWorkerInTab(tab, url) {
  info("Terminating worker with url '" + url + "' in tab.");

  return SpecialPowers.spawn(tab.linkedBrowser, [url], urlChild => {
    content.workers[urlChild].terminate();
    delete content.workers[urlChild];
  });
}

function postMessageToWorkerInTab(tab, url, message) {
  info("Posting message to worker with url '" + url + "' in tab.");

  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [url, message],
    (urlChild, messageChild) => {
      return new Promise(function(resolve) {
        const worker = content.workers[urlChild];
        worker.postMessage(messageChild);
        worker.addEventListener(
          "message",
          function() {
            resolve();
          },
          { once: true }
        );
      });
    }
  );
}

function evalInTab(tab, string) {
  info("Evalling string in tab.");

  return SpecialPowers.spawn(tab.linkedBrowser, [string], stringChild => {
    return content.eval(stringChild);
  });
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

async function waitForWorkerClose(workerDescriptorFront) {
  info("Waiting for worker to close.");
  await workerDescriptorFront.once("descriptor-destroyed");
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
  const target = await createAndAttachTargetForTab(tab);
  const { client } = target;

  await createWorkerInTab(tab, WORKER_URL);

  const { workers } = await listWorkers(target);
  const workerDescriptorFront = findWorker(workers, WORKER_URL);

  const toolbox = await gDevTools.showToolbox(
    workerDescriptorFront,
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
    workerDescriptorFront,
    toolbox,
    gDebugger,
  };
}

// Override addTab/removeTab as defined by shared-head, since these have
// an extra window parameter and add a frame script
this.addTab = function addTab(url, win) {
  info("Adding tab: " + url);
  return new Promise(resolve => {
    const targetWindow = win || window;
    const targetBrowser = targetWindow.gBrowser;

    targetWindow.focus();
    const tab = (targetBrowser.selectedTab = BrowserTestUtils.addTab(
      targetBrowser,
      url
    ));
    const linkedBrowser = tab.linkedBrowser;

    BrowserTestUtils.browserLoaded(linkedBrowser).then(function() {
      info("Tab added and finished loading: " + url);
      resolve(tab);
    });
  });
};

this.removeTab = function removeTab(tab, win) {
  info("Removing tab.");
  return new Promise(resolve => {
    const targetWindow = win || window;
    const targetBrowser = targetWindow.gBrowser;
    const tabContainer = targetBrowser.tabContainer;

    tabContainer.addEventListener(
      "TabClose",
      function() {
        info("Tab removed and finished closing.");
        resolve();
      },
      { once: true }
    );

    targetBrowser.removeTab(tab);
  });
};

function pushPrefs(...aPrefs) {
  return new Promise(resolve => {
    SpecialPowers.pushPrefEnv({ set: aPrefs }, resolve);
  });
}

function popPrefs() {
  return new Promise(resolve => {
    SpecialPowers.popPrefEnv(resolve);
  });
}
