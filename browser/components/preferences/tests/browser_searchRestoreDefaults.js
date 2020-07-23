/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { SearchTestUtils } = ChromeUtils.import(
  "resource://testing-common/SearchTestUtils.jsm"
);
const { SearchUtils } = ChromeUtils.import(
  "resource://gre/modules/SearchUtils.jsm"
);
add_task(async function test_restore_functionality() {
  // Ensure no engines are hidden to begin with.
  for (let engine of await Services.search.getDefaultEngines()) {
    if (engine.hidden) {
      engine.hidden = false;
    }
  }

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let restoreDefaultsButton = doc.getElementById("restoreDefaultSearchEngines");

  Assert.ok(
    restoreDefaultsButton.disabled,
    "Should have disabled the restore default search engines button on open"
  );

  let updatedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  let tree = doc.querySelector("#engineList");
  // Check for default search engines to be displayed in the engineList
  let defaultEngines = await Services.search.getDefaultEngines();
  for (let i = 0; i < defaultEngines.length; i++) {
    let cellName = tree.view.getCellText(
      i,
      tree.columns.getNamedColumn("engineName")
    );
    if (cellName == "DuckDuckGo") {
      tree.view.selection.select(i);
      break;
    }
  }
  doc.getElementById("removeEngineButton").click();
  await updatedPromise;

  let engine = await Services.search.getEngineByName("DuckDuckGo");

  Assert.ok(engine.hidden, "Should have hidden the engine");
  Assert.ok(
    !restoreDefaultsButton.disabled,
    "Should have enabled the restore default search engines button"
  );

  updatedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  restoreDefaultsButton.click();
  await updatedPromise;
  // Let the stack unwind so that the restore defaults button can update its
  // state.
  await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

  Assert.ok(!engine.hidden, "Should have re-enabled the disabled engine");
  Assert.ok(
    restoreDefaultsButton.disabled,
    "Should have disabled the restore default search engines button after use"
  );

  gBrowser.removeCurrentTab();
});

add_task(async function test_restoreEnabledOnOpenWithEngineHidden() {
  let engine = await Services.search.getEngineByName("DuckDuckGo");
  engine.hidden = true;

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let restoreDefaultsButton = doc.getElementById("restoreDefaultSearchEngines");

  Assert.ok(
    !restoreDefaultsButton.disabled,
    "Should have enabled the restore default search engines button on open"
  );

  let updatedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  restoreDefaultsButton.click();
  await updatedPromise;
  // Let the stack unwind so that the restore defaults button can update its
  // state.
  await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

  Assert.ok(!engine.hidden, "Should have re-enabled the disabled engine");
  Assert.ok(
    restoreDefaultsButton.disabled,
    "Should have disabled the restore default search engines button after use"
  );

  gBrowser.removeCurrentTab();
});
