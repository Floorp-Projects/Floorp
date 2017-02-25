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

  yield testErrorOnRequire(directorManager);
  yield testErrorOnEvaluate(directorManager);
  yield testErrorOnAttach(directorManager);
  yield testErrorOnDetach(directorManager);

  yield client.close();
  gBrowser.removeCurrentTab();
  DirectorRegistry.clear();
});

function* testErrorOnRequire(directorManager) {
  // director script require method should raise a "not implemented" exception
  let errorOnRequire = yield installAndEnableDirectorScript(directorManager, {
    scriptId: "testDirectorScript_errorOnRequire",
    scriptCode: "(" + (function () {
      // this director script should generate an error event
      // because require raise a "not implemented" exception
      require("fake_module");
    }).toString() + ")();",
    scriptOptions: {}
  });

  assertIsDirectorScriptError(errorOnRequire);

  let { message } = errorOnRequire;
  is(message, "Error: NOT IMPLEMENTED",
    "error.message contains the expected error message");
}

function* testErrorOnEvaluate(directorManager) {
  // director scripts should send an error events if the director script
  // raise an exception on evaluation
  let errorOnEvaluate = yield installAndEnableDirectorScript(directorManager, {
    scriptId: "testDirectorScript_errorOnEvaluate",
    scriptCode: "(" + (function () {
      // this will raise an exception evaluating
      // the director script
      raise.an_error.during.content_script.load();
    }).toString() + ")();",
    scriptOptions: {}
  });
  assertIsDirectorScriptError(errorOnEvaluate);
}

function* testErrorOnAttach(directorManager) {
  // director scripts should send an error events if the director script
  // raise an exception on evaluation
  let errorOnAttach = yield installAndEnableDirectorScript(directorManager, {
    scriptId: "testDirectorScript_errorOnAttach",
    scriptCode: "(" + (function () {
      // this will raise an exception on evaluating
      // the director script
      module.exports = function () {
        raise.an_error.during.content_script.load();
      };
    }).toString() + ")();",
    scriptOptions: {}
  });
  assertIsDirectorScriptError(errorOnAttach);
}

function* testErrorOnDetach(directorManager) {
  // director scripts should send an error events if the director script
  // raise an exception on evaluation
  yield installAndEnableDirectorScript(directorManager, {
    scriptId: "testDirectorScript_errorOnDetach",
    scriptCode: "(" + (function () {
      module.exports = function ({onUnload}) {
        // this will raise an exception on unload the director script
        onUnload(function () {
          raise_an_error_onunload();
        });
      };
    }).toString() + ")();",
    scriptOptions: {}
  });

  let waitForDetach = once(directorManager, "director-script-detach");
  let waitForError = once(directorManager, "director-script-error");

  directorManager.disableByScriptIds(["testDirectorScript_errorOnDetach"],
                                    {reload: false});

  let detach = yield waitForDetach;
  let error = yield waitForError;
  ok(detach, "testDirectorScript_errorOnDetach detach event received");
  ok(error, "testDirectorScript_errorOnDetach detach error received");
  assertIsDirectorScriptError(error);
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

function assertIsDirectorScriptError(error) {
  ok(!!error.message, "errors should contain a message");
  ok(!!error.stack, "errors should contain a stack trace");
  ok(!!error.fileName, "errors should contain a fileName");
  ok(typeof error.columnNumber == "number", "errors should contain a columnNumber");
  ok(typeof error.lineNumber == "number", "errors should contain a lineNumber");

  ok(!!error.directorScriptId, "errors should contain a directorScriptId");
}
