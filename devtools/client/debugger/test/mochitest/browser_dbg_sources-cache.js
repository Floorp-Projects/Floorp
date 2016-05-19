/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the sources cache knows how to cache sources when prompted.
 */

const TAB_URL = EXAMPLE_URL + "doc_function-search.html";
const TOTAL_SOURCES = 4;

function test() {
  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    const gTab = aTab;
    const gDebuggee = aDebuggee;
    const gPanel = aPanel;
    const gDebugger = gPanel.panelWin;
    const gEditor = gDebugger.DebuggerView.editor;
    const gSources = gDebugger.DebuggerView.Sources;
    const gPrevLabelsCache = gDebugger.SourceUtils._labelsCache;
    const gPrevGroupsCache = gDebugger.SourceUtils._groupsCache;
    const getState = gDebugger.DebuggerController.getState;
    const queries = gDebugger.require("./content/queries");
    const actions = bindActionCreators(gPanel);

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
         "There should be " + TOTAL_SOURCES + " attachments stored in the sources container model.");
      is(gSources.values.length, TOTAL_SOURCES,
         "There should be " + TOTAL_SOURCES + " values stored in the sources container model.");

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

    function performReloadAndTestState() {
      gDebugger.gTarget.once("will-navigate", testStateBeforeReload);
      gDebugger.gTarget.once("navigate", testStateAfterReload);
      return reloadActiveTab(gPanel, gDebugger.EVENTS.SOURCE_SHOWN);
    }

    function testCacheIntegrity(cachedSources) {
      const contents = {
        [EXAMPLE_URL + "code_function-search-01.js"]: "First source!",
        [EXAMPLE_URL + "code_function-search-02.js"]: "Second source!",
        [EXAMPLE_URL + "code_function-search-03.js"]: "Third source!",
        [EXAMPLE_URL + "doc_function-search.html"]: "Peanut butter jelly time!"
      };

      const sourcesText = getState().sources.sourcesText;
      is(Object.keys(sourcesText).length, cachedSources.length,
         "The right number of sources is cached");

      cachedSources.forEach(sourceUrl => {
        const source = queries.getSourceByURL(getState(), EXAMPLE_URL + sourceUrl);
        const content = queries.getSourceText(getState(), source.actor);
        ok(content, "Source text is cached");
        ok(content.text.includes(contents[source.url]), "Source text is correct");
      });
    }

    function fetchAllSources() {
      const sources = queries.getSources(getState());
      return Promise.all(Object.keys(sources).map(k => {
        const source = sources[k];
        return actions.loadSourceText(source);
      }));
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

    Task.spawn(function* () {
      yield waitForSourceShown(gPanel, "-01.js");
      yield initialChecks();
      yield testCacheIntegrity(["code_function-search-01.js"]);
      yield fetchAllSources();
      yield testCacheIntegrity([
        "code_function-search-01.js",
        "code_function-search-02.js",
        "code_function-search-03.js",
        "doc_function-search.html"
      ]);
      yield performReloadAndTestState();
      closeDebuggerAndFinish(gPanel);
    });
  });
}
