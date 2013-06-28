/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TAB_URL = EXAMPLE_URL + "browser_dbg_function-search-01.html";

/**
 * Tests if the sources cache knows how to cache sources when prompted.
 */

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;
let gEditor = null;
let gSources = null;
let gControllerSources = null;
let gPrevLabelsCache = null;
let gPrevGroupsCache = null;
const TOTAL_SOURCES = 3;

requestLongerTimeout(2);

function test()
{
  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.panelWin;

    gDebugger.addEventListener("Debugger:SourceShown", function _onEvent(aEvent) {
      gDebugger.removeEventListener(aEvent.type, _onEvent);
      Services.tm.currentThread.dispatch({ run: testSourcesCache }, 0);
    });
  });
}

function testSourcesCache()
{
  gEditor = gDebugger.DebuggerView.editor;
  gSources = gDebugger.DebuggerView.Sources;
  gControllerSources = gDebugger.DebuggerController.SourceScripts;

  ok(gEditor.getText().contains("First source!"),
    "Editor text contents appears to be correct.");
  is(gSources.selectedLabel, "test-function-search-01.js",
    "The currently selected label in the sources container is correct.");
  ok(gSources.selectedValue.contains("test-function-search-01.js"),
    "The currently selected value in the sources container appears to be correct.");

  is(gSources.itemCount, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " sources present in the sources list.");
  is(gSources.visibleItems.length, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " sources visible in the sources list.");
  is(gSources.labels.length, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " labels stored in the sources container model.")
  is(gSources.values.length, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " values stored in the sources container model.")

  info("Source labels: " + gSources.labels.toSource());
  info("Source values: " + gSources.values.toSource());

  is(gSources.labels.sort()[0], "test-function-search-01.js",
    "The first source label is correct.");
  ok(gSources.values.sort()[0].contains("test-function-search-01.js"),
    "The first source value appears to be correct.");

  is(gSources.labels.sort()[1], "test-function-search-02.js",
    "The second source label is correct.");
  ok(gSources.values.sort()[1].contains("test-function-search-02.js"),
    "The second source value appears to be correct.");

  is(gSources.labels.sort()[2], "test-function-search-03.js",
    "The third source label is correct.");
  ok(gSources.values.sort()[2].contains("test-function-search-03.js"),
    "The third source value appears to be correct.");

  is(gDebugger.SourceUtils._labelsCache.size, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " labels cached");
  is(gDebugger.SourceUtils._groupsCache.size, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " groups cached");

  gPrevLabelsCache = gDebugger.SourceUtils._labelsCache;
  gPrevGroupsCache = gDebugger.SourceUtils._groupsCache;

  fetchSources(function() {
    performReload(function() {
      closeDebuggerAndFinish();
    });
  });
}

function fetchSources(callback) {
  gControllerSources.getTextForSources(gSources.values).then((aSources) => {
    testCacheIntegrity(aSources);
    callback();
  });
}

function performReload(callback) {
  gDebugger.DebuggerController._target.once("will-navigate", testStateBeforeReload);
  gDebugger.DebuggerController._target.once("navigate", function onTabNavigated(aEvent, aPacket) {
    ok(true, "tabNavigated event was fired.");
    info("Still attached to the tab.");

    testStateAfterReload();
    callback();
  });

  gDebuggee.location.reload();
}

function testStateBeforeReload() {
  is(gSources.itemCount, 0,
    "There should be no sources present in the sources list during reload.");
  is(gControllerSources.getCache().length, 0,
    "The sources cache should be empty during reload.");
  is(gDebugger.SourceUtils._labelsCache, gPrevLabelsCache,
    "The labels cache has been refreshed during reload and no new objects were created.");
  is(gDebugger.SourceUtils._groupsCache, gPrevGroupsCache,
    "The groups cache has been refreshed during reload and no new objects were created.");
  is(gDebugger.SourceUtils._labelsCache.size, 0,
    "There should be no labels cached during reload");
  is(gDebugger.SourceUtils._groupsCache.size, 0,
    "There should be no groups cached during reload");
}

function testStateAfterReload() {
  is(gSources.itemCount, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " sources present in the sources list.");
  is(gDebugger.SourceUtils._labelsCache.size, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " labels cached after reload.");
  is(gDebugger.SourceUtils._groupsCache.size, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " groups cached after reload.");
}

function testCacheIntegrity(aCache) {
  for (let source of aCache) {
    let [url, contents] = source;

    // Sources of a debugee don't always finish fetching consecutively. D'uh.
    let index = url.match(/test-function-search-0(\d)/).pop();

    ok(index >= 1 && index <= TOTAL_SOURCES,
      "Found a source url cached correctly (" + index + ")");
    ok(contents.contains(
      ["First source!", "Second source!", "Third source!"][index - 1]),
      "Found a source's text contents cached correctly (" + index + ")");

    info("Cached source url at " + index + ": " + url);
    info("Cached source text at " + index + ": " + contents);
  }
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebuggee = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gControllerSources = null;
  gPrevLabelsCache = null;
  gPrevGroupsCache = null;
});
