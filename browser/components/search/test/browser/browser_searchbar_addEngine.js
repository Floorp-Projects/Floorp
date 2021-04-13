/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the Add Search Engine option in the search bar.
 */

"use strict";

const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

const searchPopup = document.getElementById("PopupSearchAutoComplete");
let searchbar;

add_task(async function setup() {
  searchbar = await gCUITestUtils.addSearchBar();

  registerCleanupFunction(async function() {
    gCUITestUtils.removeSearchBar();
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

  let addEngineList = searchPopup.querySelector(".search-add-engines");
  let item = addEngineList.lastElementChild;

  Assert.ok(
    item.tooltipText.includes("engineInvalid"),
    "Last item should be the invalid entry"
  );

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
