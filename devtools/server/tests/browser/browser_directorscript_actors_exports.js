/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {DirectorManagerFront} = require("devtools/shared/fronts/director-manager");
const {DirectorRegistry} = require("devtools/server/actors/director-registry");

DirectorRegistry.clear();

add_task(function* () {
  let browser = yield addTab(MAIN_DOMAIN + "director-script-target.html");
  let doc = browser.contentDocument;

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);

  DirectorRegistry.clear();
  let directorManager = DirectorManagerFront(client, form);

  // director scripts attach method defaults to module.exports
  let attachModuleExports = yield testDirectorScriptExports(directorManager, {
    scriptId: "testDirectorScript_moduleExports",
    scriptCode: "(" + (function () {
      module.exports = function () {};
    }).toString() + ")();",
    scriptOptions: {}
  });
  ok(attachModuleExports.port, "testDirectorScript_moduleExports attach event received");

  // director scripts attach method can be configured using the attachMethod scriptOptions
  let attachExportsAttach = yield testDirectorScriptExports(directorManager, {
    scriptId: "testDirectorScript_exportsAttach",
    scriptCode: "(" + (function () {
      exports.attach = function () {};
    }).toString() + ")();",
    scriptOptions: {
      attachMethod: "attach"
    }
  });
  ok(attachExportsAttach.port, "testDirectorScript_exportsAttach attach event received");

  // director scripts without an attach method generates an error event
  let errorUndefinedAttachMethod = yield testDirectorScriptExports(directorManager, {
    scriptId: "testDirectorScript_undefinedAttachMethod",
    scriptCode: "(" + (function () {
      // this director script should raise an error
      // because it doesn't export any attach method
    }).toString() + ")();",
    scriptOptions: {
      attachMethod: "attach"
    }
  });
  let { message } = errorUndefinedAttachMethod;
  ok(!!message, "testDirectorScript_undefinedAttachMethod error event received");
  assertIsDirectorScriptError(errorUndefinedAttachMethod);

  yield client.close();
  gBrowser.removeCurrentTab();
  DirectorRegistry.clear();
});

function assertIsDirectorScriptError(error) {
  ok(!!error.message, "errors should contain a message");
  ok(!!error.stack, "errors should contain a stack trace");
  ok(!!error.fileName, "errors should contain a fileName");
  ok(typeof error.columnNumber == "number", "errors should contain a columnNumber");
  ok(typeof error.lineNumber == "number", "errors should contain a lineNumber");

  ok(!!error.directorScriptId, "errors should contain a directorScriptId");
}

function* testDirectorScriptExports(directorManager, directorScriptDef) {
  let { scriptId } = directorScriptDef;

  DirectorRegistry.install(scriptId, directorScriptDef);

  let waitForAttach = once(directorManager, "director-script-attach");
  let waitForError = once(directorManager, "director-script-error");
  directorManager.enableByScriptIds([scriptId], {reload: false});

  let attachOrErrorEvent = yield Promise.race([waitForAttach, waitForError]);

  return attachOrErrorEvent;
}
