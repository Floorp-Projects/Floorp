/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests input history in cases where the search string is empty.
 * In the future we may want to not account for these, but for now they are
 * stored with an empty input field.
 */

"use strict";

async function checkInputHistory(len = 0) {
  await PlacesUtils.withConnectionWrapper(
    "test::checkInputHistory",
    async db => {
      let rows = await db.executeCached(`SELECT input FROM moz_inputhistory`);
      Assert.equal(rows.length, len, "There should only be 1 entry");
      if (len) {
        Assert.equal(rows[0].getResultByIndex(0), "", "Input should be empty");
      }
    }
  );
}

const TEST_URL = "http://example.com/";

async function do_test(openFn, pickMethod) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    async function (browser) {
      await PlacesTestUtils.clearInputHistory();
      await openFn();
      await UrlbarTestUtils.promiseSearchComplete(window);
      let promise = BrowserTestUtils.waitForDocLoadAndStopIt(TEST_URL, browser);
      if (pickMethod == "keyboard") {
        info(`Test pressing Enter`);
        EventUtils.sendKey("down");
        EventUtils.sendKey("return");
      } else {
        info("Test with click");
        let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
        EventUtils.synthesizeMouseAtCenter(result.element.row, {});
      }
      await promise;
      await checkInputHistory(1);
    }
  );
}

add_setup(async function () {
  await PlacesUtils.history.clear();
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits(TEST_URL);
  }

  await updateTopSites(sites => sites && sites[0] && sites[0].url == TEST_URL);
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_history_no_search_terms() {
  for (let pickMethod of ["keyboard", "mouse"]) {
    // If a testFn returns false, it will be skipped.
    for (let openFn of [
      () => {
        info("Test opening panel with down key");
        gURLBar.focus();
        EventUtils.sendKey("down");
      },
      async () => {
        info("Test opening panel on focus");
        gURLBar.blur();
        EventUtils.synthesizeMouseAtCenter(gURLBar.textbox, {});
      },
      async () => {
        info("Test opening panel on focus on a page");
        let selectedBrowser = gBrowser.selectedBrowser;
        // A page other than TEST_URL must be loaded, or the first Top Site
        // result will be a switch-to-tab result and page won't be reloaded when
        // the result is selected.
        BrowserTestUtils.startLoadingURIString(
          selectedBrowser,
          "http://example.org/"
        );
        await BrowserTestUtils.browserLoaded(selectedBrowser);
        gURLBar.blur();
        EventUtils.synthesizeMouseAtCenter(gURLBar.textbox, {});
      },
    ]) {
      await do_test(openFn, pickMethod);
    }
  }
});
