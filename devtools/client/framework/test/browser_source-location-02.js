/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the SourceLocationController updates generated sources when pretty printing
 * and un pretty printing.
 */

const DEBUGGER_ROOT = "http://example.com/browser/devtools/client/debugger/test/mochitest/";
// Empty page
const PAGE_URL = `${DEBUGGER_ROOT}doc_empty-tab-01.html`;
const JS_URL = `${URL_ROOT}code_ugly.js`;
const { SourceLocationController } = require("devtools/client/framework/source-location");

add_task(function* () {
  let toolbox = yield openNewTabAndToolbox(PAGE_URL, "jsdebugger");

  let controller = new SourceLocationController(toolbox.target);

  let checkedPretty = false;
  let checkedUnpretty = false;

  function onUpdate(oldLoc, newLoc) {
    if (oldLoc.line === 3) {
      checkPrettified(oldLoc, newLoc);
      checkedPretty = true;
    } else if (oldLoc.line === 9) {
      checkUnprettified(oldLoc, newLoc);
      checkedUnpretty = true;
    } else {
      throw new Error(`Unexpected location update: ${JSON.stringify(oldLoc)}`);
    }
  }

  controller.bindLocation({ url: JS_URL, line: 3 }, onUpdate);

  // Inject JS script
  let sourceShown = waitForSourceShown(toolbox.getCurrentPanel(), "code_ugly.js");
  yield createScript(JS_URL);
  yield sourceShown;

  let ppButton = toolbox.getCurrentPanel().panelWin.document.getElementById("pretty-print");
  sourceShown = waitForSourceShown(toolbox.getCurrentPanel(), "code_ugly.js");
  ppButton.click();
  yield sourceShown;
  yield waitUntil(() => checkedPretty);

  // TODO check unprettified change once bug 1177446 fixed
  /*
  sourceShown = waitForSourceShown(toolbox.getCurrentPanel(), "code_ugly.js");
  ppButton.click();
  yield sourceShown;
  yield waitUntil(() => checkedUnpretty);
  */

  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
  finish();
});

function checkPrettified(oldLoc, newLoc) {
  is(oldLoc.line, 3, "Correct line for JS:3");
  is(oldLoc.column, null, "Correct column for JS:3");
  is(oldLoc.url, JS_URL, "Correct url for JS:3");
  is(newLoc.line, 9, "Correct line for JS:3 -> PRETTY");
  is(newLoc.column, 0, "Correct column for JS:3 -> PRETTY");
  is(newLoc.url, JS_URL, "Correct url for JS:3 -> PRETTY");
}

function checkUnprettified(oldLoc, newLoc) {
  is(oldLoc.line, 9, "Correct line for JS:3 -> PRETTY");
  is(oldLoc.column, 0, "Correct column for JS:3 -> PRETTY");
  is(oldLoc.url, JS_URL, "Correct url for JS:3 -> PRETTY");
  is(newLoc.line, 3, "Correct line for JS:3 -> UNPRETTIED");
  is(newLoc.column, null, "Correct column for JS:3 -> UNPRETTIED");
  is(newLoc.url, JS_URL, "Correct url for JS:3 -> UNPRETTIED");
}

function createScript(url) {
  info(`Creating script: ${url}`);
  let mm = getFrameScript();
  let command = `
    let script = document.createElement("script");
    script.setAttribute("src", "${url}");
    document.body.appendChild(script);
  `;
  return evalInDebuggee(mm, command);
}

function waitForSourceShown(debuggerPanel, url) {
  let { panelWin } = debuggerPanel;
  let deferred = defer();

  info(`Waiting for source ${url} to be shown in the debugger...`);
  panelWin.on(panelWin.EVENTS.SOURCE_SHOWN, function onSourceShown(_, source) {
    let sourceUrl = source.url || source.introductionUrl;

    if (sourceUrl.includes(url)) {
      panelWin.off(panelWin.EVENTS.SOURCE_SHOWN, onSourceShown);
      info(`Source shown for ${url}`);
      deferred.resolve(source);
    }
  });

  return deferred.promise;
}
