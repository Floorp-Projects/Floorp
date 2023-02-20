/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);
const { SearchUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchUtils.sys.mjs"
);
add_task(async function test_restore_functionality() {
  // Ensure no engines are hidden to begin with.
  for (let engine of await Services.search.getAppProvidedEngines()) {
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
  let defaultEngines = await Services.search.getAppProvidedEngines();
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

// This removes the last two engines and then the remaining engines from top to
// bottom, and then it restores the default engines.  See bug 1681818.
add_task(async function test_removeOutOfOrder() {
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let restoreDefaultsButton = doc.getElementById("restoreDefaultSearchEngines");
  Assert.ok(
    restoreDefaultsButton.disabled,
    "The restore-defaults button is disabled initially"
  );

  let tree = doc.querySelector("#engineList");
  let removeEngineButton = doc.getElementById("removeEngineButton");
  removeEngineButton.scrollIntoView();

  let defaultEngines = await Services.search.getAppProvidedEngines();

  // Remove the last two engines.  After each removal, the selection should move
  // to the first local shortcut.
  for (let i = 0; i < 2; i++) {
    tree.view.selection.select(defaultEngines.length - i - 1);
    let updatedPromise = SearchTestUtils.promiseSearchNotification(
      SearchUtils.MODIFIED_TYPE.CHANGED,
      SearchUtils.TOPIC_ENGINE_MODIFIED
    );
    removeEngineButton.click();
    await updatedPromise;
    Assert.ok(
      removeEngineButton.disabled,
      "The remove-engine button is disabled because a local shortcut is selected"
    );
    Assert.ok(
      !restoreDefaultsButton.disabled,
      "The restore-defaults button is enabled after removing an engine"
    );
  }

  // Remove the remaining engines from top to bottom except for the default engine
  // which can't be removed.
  for (let i = 0; i < defaultEngines.length - 3; i++) {
    tree.view.selection.select(0);

    if (defaultEngines[0].name == Services.search.defaultEngine.name) {
      tree.view.selection.select(1);
    }

    let updatedPromise = SearchTestUtils.promiseSearchNotification(
      SearchUtils.MODIFIED_TYPE.CHANGED,
      SearchUtils.TOPIC_ENGINE_MODIFIED
    );
    removeEngineButton.click();
    await updatedPromise;
    Assert.ok(
      !restoreDefaultsButton.disabled,
      "The restore-defaults button is enabled after removing an engine"
    );
  }
  Assert.ok(
    removeEngineButton.disabled,
    "The remove-engine button is disabled because only one engine remains"
  );

  // Click the restore-defaults button.
  let updatedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  restoreDefaultsButton.click();
  await updatedPromise;

  // Wait for the restore-defaults button to update its state.
  await TestUtils.waitForCondition(
    () => restoreDefaultsButton.disabled,
    "Waiting for the restore-defaults button to become disabled"
  );

  Assert.ok(
    restoreDefaultsButton.disabled,
    "The restore-defaults button is disabled after restoring defaults"
  );
  Assert.equal(
    tree.view.rowCount,
    defaultEngines.length + UrlbarUtils.LOCAL_SEARCH_MODES.length,
    "All engines are restored"
  );

  gBrowser.removeCurrentTab();
});

add_task(async function test_removeAndRestoreMultiple() {
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let restoreDefaultsButton = doc.getElementById("restoreDefaultSearchEngines");
  let tree = doc.querySelector("#engineList");
  let removeEngineButton = doc.getElementById("removeEngineButton");
  removeEngineButton.scrollIntoView();

  let defaultEngines = await Services.search.getAppProvidedEngines();

  // Remove the second and fourth engines.
  for (let i = 0; i < 2; i++) {
    tree.view.selection.select(i * 2 + 1);
    let updatedPromise = SearchTestUtils.promiseSearchNotification(
      SearchUtils.MODIFIED_TYPE.CHANGED,
      SearchUtils.TOPIC_ENGINE_MODIFIED
    );
    removeEngineButton.click();
    await updatedPromise;
  }

  // Click the restore-defaults button.
  let updatedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  restoreDefaultsButton.click();
  await updatedPromise;

  // Remove the third engine.
  tree.view.selection.select(3);
  updatedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  removeEngineButton.click();
  await updatedPromise;

  // Now restore again.
  updatedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  restoreDefaultsButton.click();
  await updatedPromise;

  // Wait for the restore-defaults button to update its state.
  await TestUtils.waitForCondition(
    () => restoreDefaultsButton.disabled,
    "Waiting for the restore-defaults button to become disabled"
  );

  Assert.equal(
    tree.view.rowCount,
    defaultEngines.length + UrlbarUtils.LOCAL_SEARCH_MODES.length,
    "Should have the correct amount of engines"
  );

  gBrowser.removeCurrentTab();
});
