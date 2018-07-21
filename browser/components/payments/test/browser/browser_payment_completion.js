"use strict";

/*
  Test the permutations of calling complete() on the payment response and handling the case
  where the timeout is exceeded before it is called
*/

async function setup() {
  await setupFormAutofillStorage();
  await cleanupFormAutofillStorage();
  let billingAddressGUID = await addAddressRecord(PTU.Addresses.TimBL);
  let card = Object.assign({}, PTU.BasicCards.JohnDoe,
                           { billingAddressGUID });
  await addCardRecord(card);
}

add_task(async function test_complete_success() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.total60USD),
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let {completeException} = await ContentTask.spawn(browser,
                                                      { result: "success" },
                                                      PTU.ContentTasks.addCompletionHandler);

    ok(!completeException, "Expect no exception to be thrown when calling complete()");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_complete_fail() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.total60USD),
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    info("acknowledging the completion from the merchant page");
    let {completeException} = await ContentTask.spawn(browser,
                                                      { result: "fail" },
                                                      PTU.ContentTasks.addCompletionHandler);
    ok(!completeException, "Expect no exception to be thrown when calling complete()");

    ok(!win.closed, "dialog shouldn't be closed yet");

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.clickPrimaryButton);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_complete_timeout() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    // timeout the response asap
    Services.prefs.setIntPref(RESPONSE_TIMEOUT_PREF, 60);

    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.total60USD),
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    info("clicking pay");
    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    info("acknowledging the completion from the merchant page after a delay");
    let {completeException} = await ContentTask.spawn(browser,
                                                      { result: "fail", delayMs: 1000 },
                                                      PTU.ContentTasks.addCompletionHandler);
    ok(completeException,
       "Expect an exception to be thrown when calling complete() too late");

    ok(!win.closed, "dialog shouldn't be closed");

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.clickPrimaryButton);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
