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

const DEBUGGER_ROOT = "http://example.com/browser/devtools/client/debugger/test/mochitest/";
// Empty page
const PAGE_URL = `${DEBUGGER_ROOT}doc_empty-tab-01.html`;
const JS_URL = `${URL_ROOT}code_binary_search.js`;
const COFFEE_URL = `${URL_ROOT}code_binary_search.coffee`;
const { SourceMapService } = require("devtools/client/framework/source-map-service");

add_task(function* () {
  const toolbox = yield openNewTabAndToolbox(PAGE_URL, "jsdebugger");

  const service = new SourceMapService(toolbox.target);

  const aggregator = [];

  function onUpdate(e, oldLoc, newLoc) {
    if (oldLoc.line === 6) {
      checkLoc1(oldLoc, newLoc);
    } else if (oldLoc.line === 8) {
      checkLoc2(oldLoc, newLoc);
    } else if (oldLoc.line === 2) {
      checkLoc3(oldLoc, newLoc);
    } else {
      throw new Error(`Unexpected location update: ${JSON.stringify(oldLoc)}`);
    }
    aggregator.push(newLoc);
  }

  let loc1 = { url: JS_URL, line: 6 };
  let loc2 = { url: JS_URL, line: 8, column: 3 };

  service.subscribe(loc1, onUpdate);
  service.subscribe(loc2, onUpdate);

  // Inject JS script
  let sourceShown = waitForSourceShown(toolbox.getCurrentPanel(), "code_binary_search");
  yield createScript(JS_URL);
  yield sourceShown;

  yield waitUntil(() => aggregator.length === 2);

  ok(aggregator.find(i => i.url === COFFEE_URL && i.line === 4), "found first updated location");
  ok(aggregator.find(i => i.url === COFFEE_URL && i.line === 6), "found second updated location");

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
  is(newLoc.url, COFFEE_URL, "Correct url for JS:6 -> COFFEE");
}

function checkLoc2(oldLoc, newLoc) {
  is(oldLoc.line, 8, "Correct line for JS:8:3");
  is(oldLoc.column, 3, "Correct column for JS:8:3");
  is(oldLoc.url, JS_URL, "Correct url for JS:8:3");
  is(newLoc.line, 6, "Correct line for JS:8:3 -> COFFEE");
  is(newLoc.column, 10, "Correct column for JS:8:3 -> COFFEE");
  is(newLoc.url, COFFEE_URL, "Correct url for JS:8:3 -> COFFEE");
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
