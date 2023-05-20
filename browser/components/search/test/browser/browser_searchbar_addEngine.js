/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the Add Search Engine option in the search bar.
 */

"use strict";

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

const searchPopup = document.getElementById("PopupSearchAutoComplete");
let searchbar;

add_setup(async function () {
  searchbar = await gCUITestUtils.addSearchBar();

  registerCleanupFunction(async function () {
    gCUITestUtils.removeSearchBar();
    Services.search.restoreDefaultEngines();
  });
});

add_task(async function test_invalidEngine() {
  let rootDir = getRootDirectory(gTestPath);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    rootDir + "opensearch.html"
  );
  let promise = promiseEvent(searchPopup, "popupshown");
  await EventUtils.synthesizeMouseAtCenter(
    searchbar.querySelector(".searchbar-search-button"),
    {}
  );
  await promise;

  let addEngineList = searchPopup.querySelectorAll(
    ".searchbar-engine-one-off-add-engine"
  );
  let item = addEngineList[addEngineList.length - 1];

  await TestUtils.waitForCondition(
    () => item.tooltipText.includes("engineInvalid"),
    "Wait until the tooltip will be correct"
  );
  Assert.ok(true, "Last item should be the invalid entry");

  let promptPromise = PromptTestUtils.waitForPrompt(tab.linkedBrowser, {
    modalType: Ci.nsIPromptService.MODAL_TYPE_CONTENT,
    promptType: "alert",
  });

  await EventUtils.synthesizeMouseAtCenter(item, {});

  let prompt = await promptPromise;

  Assert.ok(
    prompt.ui.infoBody.textContent.includes(
      "http://mochi.test:8888/browser/browser/components/search/test/browser/testEngine_404.xml"
    ),
    "Should have included the url in the prompt body"
  );

  await PromptTestUtils.handlePrompt(prompt);
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_onOnlyDefaultEngine() {
  info("Remove engines except default");
  const defaultEngine = Services.search.defaultEngine;
  const engines = await Services.search.getVisibleEngines();
  for (const engine of engines) {
    if (defaultEngine.name !== engine.name) {
      await Services.search.removeEngine(engine);
    }
  }

  info("Show popup");
  const rootDir = getRootDirectory(gTestPath);
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    rootDir + "opensearch.html"
  );
  const onShown = promiseEvent(searchPopup, "popupshown");
  await EventUtils.synthesizeMouseAtCenter(
    searchbar.querySelector(".searchbar-search-button"),
    {}
  );
  await onShown;

  const addEngineList = searchPopup.querySelectorAll(
    ".searchbar-engine-one-off-add-engine"
  );
  Assert.equal(addEngineList.length, 3, "Add engines should be shown");

  BrowserTestUtils.removeTab(tab);
});
