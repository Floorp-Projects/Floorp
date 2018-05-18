"use strict";

add_task(async function test_dropdown() {
  await addSampleAddressesAndBasicCard();

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} = await setupPaymentDialog(browser, {
      details: PTU.Details.total60USD,
      methodData: [PTU.MethodData.basicCard],
      merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
    });

    let popupset = frame.ownerDocument.querySelector("popupset");
    ok(popupset, "popupset exists");
    let popupshownPromise = BrowserTestUtils.waitForEvent(popupset, "popupshown");

    info("switch to the address add page");
    await spawnPaymentDialogTask(frame, async function changeToAddressAddPage() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let addLink = content.document.querySelector("address-picker .add-link");
      is(addLink.textContent, "Add", "Add link text");

      addLink.click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !state.page.guid;
      }, "Check add page state");
    });

    info("going to open the country <select>");
    await BrowserTestUtils.synthesizeMouseAtCenter("#country", {}, frame);

    let event = await popupshownPromise;
    let expectedPopupID = "ContentSelectDropdown";
    if (AppConstants.platform == "win") {
      expectedPopupID = "ContentSelectDropdown-windows";
    }
    is(event.target.parentElement.id, expectedPopupID, "Checked menulist of opened popup");

    info("clicking cancel");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
