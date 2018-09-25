"use strict";

const methodData = [PTU.MethodData.basicCard];
const details = Object.assign({}, PTU.Details.twoShippingOptions, PTU.Details.total2USD);

add_task(async function setup_once() {
  // add an address and card to avoid the FTU sequence
  await addSampleAddressesAndBasicCard([PTU.Addresses.TimBL],
                                       [PTU.BasicCards.JohnDoe]);
});

add_task(async function test_openPreferences() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} = await setupPaymentDialog(browser, {
      methodData,
      details,
      merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
    });

    let prefsWindowPromise = BrowserTestUtils.waitForNewWindow({
      url: "about:preferences#privacy",
    });

    let prefsLoadedPromise = TestUtils.topicObserved("sync-pane-loaded");

    await spawnPaymentDialogTask(frame, function verifyPrefsLink({isMac}) {
      let manageTextEl = content.document.querySelector(".manage-text");

      let expectedVisibleEl;
      if (isMac) {
        expectedVisibleEl = manageTextEl.querySelector(":scope > span[data-os='mac']");
        ok(manageTextEl.innerText.includes("Preferences"), "Visible string includes 'Preferences'");
        ok(!manageTextEl.innerText.includes("Options"), "Visible string includes 'Options'");
      } else {
        expectedVisibleEl = manageTextEl.querySelector(":scope > span:not([data-os='mac'])");
        ok(!manageTextEl.innerText.includes("Preferences"),
           "Visible string includes 'Preferences'");
        ok(manageTextEl.innerText.includes("Options"), "Visible string includes 'Options'");
      }

      let prefsLink = expectedVisibleEl.querySelector("a");
      ok(prefsLink, "Preferences link should exist");
      prefsLink.scrollIntoView();
      EventUtils.synthesizeMouseAtCenter(prefsLink, {}, content);
    }, {
      isMac: AppConstants.platform == "macosx",
    });

    let browserWin = await prefsWindowPromise;
    ok(browserWin, "Ensure a window was opened");
    await prefsLoadedPromise;

    await BrowserTestUtils.closeWindow(browserWin);

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
