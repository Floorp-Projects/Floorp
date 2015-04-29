/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the sources cache knows how to cache sources when prompted.
 */

const TAB_URL = EXAMPLE_URL + "doc_function-search.html";
const TOTAL_SOURCES = 4;

let gTab, gDebuggee, gPanel, gDebugger;
let gEditor, gSources, gControllerSources;
let gPrevLabelsCache, gPrevGroupsCache;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gEditor = gDebugger.DebuggerView.editor;
    gSources = gDebugger.DebuggerView.Sources;
    gControllerSources = gDebugger.DebuggerController.SourceScripts;
    gPrevLabelsCache = gDebugger.SourceUtils._labelsCache;
    gPrevGroupsCache = gDebugger.SourceUtils._groupsCache;

    waitForSourceShown(gPanel, "-01.js")
      .then(initialChecks)
      .then(getTextForSourcesAndCheckIntegrity)
      .then(performReloadAndTestState)
      .then(() => closeDebuggerAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });
  });
}

function initialChecks() {
  ok(gEditor.getText().includes("First source!"),
    "Editor text contents appears to be correct.");
  is(gSources.selectedItem.attachment.label, "code_function-search-01.js",
    "The currently selected label in the sources container is correct.");
  ok(getSelectedSourceURL(gSources).includes("code_function-search-01.js"),
    "The currently selected value in the sources container appears to be correct.");

  is(gSources.itemCount, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " sources present in the sources list.");
  is(gSources.visibleItems.length, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " sources visible in the sources list.");
  is(gSources.attachments.length, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " attachments stored in the sources container model.")
  is(gSources.values.length, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " values stored in the sources container model.")

  info("Source labels: " + gSources.attachments.toSource());
  info("Source values: " + gSources.values.toSource());

  is(gSources.attachments[0].label, "code_function-search-01.js",
    "The first source label is correct.");
  ok(gSources.attachments[0].source.url.includes("code_function-search-01.js"),
    "The first source value appears to be correct.");

  is(gSources.attachments[1].label, "code_function-search-02.js",
    "The second source label is correct.");
  ok(gSources.attachments[1].source.url.includes("code_function-search-02.js"),
    "The second source value appears to be correct.");

  is(gSources.attachments[2].label, "code_function-search-03.js",
    "The third source label is correct.");
  ok(gSources.attachments[2].source.url.includes("code_function-search-03.js"),
    "The third source value appears to be correct.");

  is(gSources.attachments[3].label, "doc_function-search.html",
    "The third source label is correct.");
  ok(gSources.attachments[3].source.url.includes("doc_function-search.html"),
    "The third source value appears to be correct.");

  is(gDebugger.SourceUtils._labelsCache.size, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " labels cached.");
  is(gDebugger.SourceUtils._groupsCache.size, TOTAL_SOURCES,
    "There should be " + TOTAL_SOURCES + " groups cached.");
}

function getTextForSourcesAndCheckIntegrity() {
  return gControllerSources.getTextForSources(gSources.values).then(testCacheIntegrity);
}

function performReloadAndTestState() {
  gDebugger.gTarget.once("will-navigate", testStateBeforeReload);
  gDebugger.gTarget.once("navigate", testStateAfterReload);
  return reloadActiveTab(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
}

function testCacheIntegrity(aSources) {
  for (let [actor, contents] of aSources) {
    // Sources of a debugee don't always finish fetching consecutively. D'uh.
    let index = gSources.values.indexOf(actor);

    ok(index >= 0 && index <= TOTAL_SOURCES,
      "Found a source actor cached correctly (" + index + ").");
    ok(contents.includes(
      ["First source!", "Second source!", "Third source!", "Peanut butter jelly time!"][index]),
      "Found a source's text contents cached correctly (" + index + ").");

    info("Cached source actor at " + index + ": " + actor);
    info("Cached source text at " + index + ": " + contents);
  }
}

function testStateBeforeReload() {
  is(gSources.itemCount, 0,
    "There should be no sources present in the sources list during reload.");
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

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gEditor = null;
  gSources = null;
  gControllerSources = null;
  gPrevLabelsCache = null;
  gPrevGroupsCache = null;
});
