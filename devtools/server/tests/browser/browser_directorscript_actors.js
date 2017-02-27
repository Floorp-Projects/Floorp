/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {DirectorManagerFront} = require("devtools/shared/fronts/director-manager");
const {DirectorRegistry} = require("devtools/server/actors/director-registry");

add_task(function* () {
  yield addTab(MAIN_DOMAIN + "director-script-target.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);

  DirectorRegistry.clear();
  let directorManager = DirectorManagerFront(client, form);

  yield testDirectorScriptAttachEventAttributes(directorManager);
  yield testDirectorScriptMessagePort(directorManager);
  yield testDirectorScriptWindowEval(directorManager);
  yield testDirectorScriptUnloadOnDetach(directorManager);

  yield client.close();
  gBrowser.removeCurrentTab();
  DirectorRegistry.clear();
});

function* testDirectorScriptAttachEventAttributes(directorManager) {
  let attachEvent = yield installAndEnableDirectorScript(directorManager, {
    scriptId: "testDirectorScript_attachEventAttributes",
    scriptCode: "(" + (function () {
      exports.attach = function () {};
    }).toString() + ")();",
    scriptOptions: {
      attachMethod: "attach"
    }
  });

  let { directorScriptId, url } = attachEvent;

  is(directorScriptId, "testDirectorScript_attachEventAttributes",
     "attach event should contains directorScriptId");
  is(url, MAIN_DOMAIN + "director-script-target.html");
}

function* testDirectorScriptMessagePort(directorManager) {
  let { port } = yield installAndEnableDirectorScript(directorManager, {
    scriptId: "testDirectorScript_MessagePort",
    scriptCode: "(" + (function () {
      exports.attach = function ({port: messagePort}) {
        messagePort.onmessage = function (evt) {
          // echo messages
          evt.target.postMessage(evt.data);
        };
      };
    }).toString() + ")();",
    scriptOptions: {
      attachMethod: "attach"
    }
  });

  ok(port && port.postMessage, "testDirector_MessagePort port received");

  // exchange messages over the MessagePort
  let waitForMessagePortEvent = once(port, "message");
  // needs to explicit start the port
  port.start();

  let msg = { k1: "v1", k2: [1, 2, 3] };
  port.postMessage(msg);

  let reply = yield waitForMessagePortEvent;

  is(JSON.stringify(reply.data), JSON.stringify(msg),
    "echo reply received on the MessagePortClient");
}

function* testDirectorScriptWindowEval(directorManager) {
  let { port } = yield installAndEnableDirectorScript(directorManager, {
    scriptId: "testDirectorScript_WindowEval",
    scriptCode: "(" + (function () {
      exports.attach = function ({window, port: evalPort}) {
        let onpageloaded = function () {
          let globalVarValue = window.eval("globalAccessibleVar;");
          evalPort.postMessage(globalVarValue);
        };

        if (window.document && window.document.readyState === "complete") {
          onpageloaded();
        } else {
          window.addEventListener("load", onpageloaded);
        }
      };
    }).toString() + ")();",
    scriptOptions: {
      attachMethod: "attach"
    }
  });

  ok(port, "testDirectorScript_WindowEval port received");

  // exchange messages over the MessagePort
  let waitForMessagePortEvent = once(port, "message");
  // needs to explicit start the port
  port.start();

  let portEvent = yield waitForMessagePortEvent;

  ok(portEvent.data !== "unsecure-eval", "window.eval should be wrapped and safe");
  is(portEvent.data, "global-value",
    "globalAccessibleVar should be accessible through window.eval");
}

function* testDirectorScriptUnloadOnDetach(directorManager) {
  let { port } = yield installAndEnableDirectorScript(directorManager, {
    scriptId: "testDirectorScript_unloadOnDetach",
    scriptCode: "(" + (function () {
      exports.attach = function ({port: unloadPort, onUnload}) {
        onUnload(function () {
          unloadPort.postMessage("ONUNLOAD");
        });
      };
    }).toString() + ")();",
    scriptOptions: {
      attachMethod: "attach"
    }
  });

  ok(port, "testDirectorScript_unloadOnDetach port received");
  port.start();

  let waitForDetach = once(directorManager, "director-script-detach");
  let waitForMessage = once(port, "message");

  directorManager.disableByScriptIds(["testDirectorScript_unloadOnDetach"],
                                    {reload: false});

  let { directorScriptId } = yield waitForDetach;
  is(directorScriptId, "testDirectorScript_unloadOnDetach",
     "detach event should contains directorScriptId");

  let portEvent = yield waitForMessage;
  is(portEvent.data, "ONUNLOAD", "director-script's exports.onUnload called on detach");
}

function* installAndEnableDirectorScript(directorManager, directorScriptDef) {
  let { scriptId } = directorScriptDef;

  DirectorRegistry.install(scriptId, directorScriptDef);

  let waitForAttach = once(directorManager, "director-script-attach");
  let waitForError = once(directorManager, "director-script-error");

  directorManager.enableByScriptIds([scriptId], {reload: false});

  let attachOrErrorEvent = yield Promise.race([waitForAttach, waitForError]);

  return attachOrErrorEvent;
}
