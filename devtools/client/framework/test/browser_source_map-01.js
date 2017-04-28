/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejections should be fixed.
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("[object Object]");
thisTestLeaksUncaughtRejectionsAndShouldBeFixed(
  "TypeError: this.transport is null");

/**
 * Tests the SourceMapService updates generated sources when source maps
 * are subsequently found. Also checks when no column is provided, and
 * when tagging an already source mapped location initially.
 */

// Force the old debugger UI since it's directly used (see Bug 1301705)
Services.prefs.setBoolPref("devtools.debugger.new-debugger-frontend", false);
registerCleanupFunction(function* () {
  Services.prefs.clearUserPref("devtools.debugger.new-debugger-frontend");
});

const DEBUGGER_ROOT = "http://example.com/browser/devtools/client/debugger/test/mochitest/";
// Empty page
const PAGE_URL = `${DEBUGGER_ROOT}doc_empty-tab-01.html`;
const JS_URL = `${URL_ROOT}code_binary_search.js`;
const COFFEE_URL = `${URL_ROOT}code_binary_search.coffee`;

add_task(function* () {
  const toolbox = yield openNewTabAndToolbox(PAGE_URL, "jsdebugger");
  const service = toolbox.sourceMapURLService;

  // Inject JS script
  let sourceShown = waitForSourceShown(toolbox.getCurrentPanel(), "code_binary_search");
  yield createScript(JS_URL);
  yield sourceShown;

  let loc1 = { url: JS_URL, line: 6 };
  let newLoc1 = yield service.originalPositionFor(loc1.url, loc1.line);
  checkLoc1(loc1, newLoc1);

  let loc2 = { url: JS_URL, line: 8, column: 3 };
  let newLoc2 = yield service.originalPositionFor(loc2.url, loc2.line, loc2.column);
  checkLoc2(loc2, newLoc2);

  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
  finish();
});

function checkLoc1(oldLoc, newLoc) {
  is(oldLoc.line, 6, "Correct line for JS:6");
  is(oldLoc.column, null, "Correct column for JS:6");
  is(oldLoc.url, JS_URL, "Correct url for JS:6");
  is(newLoc.line, 4, "Correct line for JS:6 -> COFFEE");
  is(newLoc.column, 2, "Correct column for JS:6 -> COFFEE -- handles falsy column entries");
  is(newLoc.sourceUrl, COFFEE_URL, "Correct url for JS:6 -> COFFEE");
}

function checkLoc2(oldLoc, newLoc) {
  is(oldLoc.line, 8, "Correct line for JS:8:3");
  is(oldLoc.column, 3, "Correct column for JS:8:3");
  is(oldLoc.url, JS_URL, "Correct url for JS:8:3");
  is(newLoc.line, 6, "Correct line for JS:8:3 -> COFFEE");
  is(newLoc.column, 10, "Correct column for JS:8:3 -> COFFEE");
  is(newLoc.sourceUrl, COFFEE_URL, "Correct url for JS:8:3 -> COFFEE");
}

function createScript(url) {
  info(`Creating script: ${url}`);
  let mm = getFrameScript();
  let command = `
    let script = document.createElement("script");
    script.setAttribute("src", "${url}");
    document.body.appendChild(script);
    null;
  `;
  return evalInDebuggee(mm, command);
}

function waitForSourceShown(debuggerPanel, url) {
  let { panelWin } = debuggerPanel;
  let deferred = defer();

  info(`Waiting for source ${url} to be shown in the debugger...`);
  panelWin.on(panelWin.EVENTS.SOURCE_SHOWN, function onSourceShown(_, source) {

    let sourceUrl = source.url || source.generatedUrl;
    if (sourceUrl.includes(url)) {
      panelWin.off(panelWin.EVENTS.SOURCE_SHOWN, onSourceShown);
      info(`Source shown for ${url}`);
      deferred.resolve(source);
    }
  });

  return deferred.promise;
}
