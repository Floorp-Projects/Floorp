/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests to ensure that the reason dropdown is shown or hidden
 * based on its pref, and that its optional and required modes affect
 * the Send button and report appropriately.
 */

"use strict";

add_common_setup();

async function clickSendAndCheckPing(rbs, expectedReason = null) {
  const pingCheck = new Promise(resolve => {
    GleanPings.brokenSiteReport.testBeforeNextSubmit(() => {
      Assert.equal(
        Glean.brokenSiteReport.breakageCategory.testGetValue(),
        expectedReason
      );
      resolve();
    });
  });
  await rbs.clickSend();
  return pingCheck;
}

add_task(async function testReasonDropdown() {
  ensureReportBrokenSitePreffedOn();

  await BrowserTestUtils.withNewTab(
    REPORTABLE_PAGE_URL,
    async function (browser) {
      ensureReasonDisabled();

      let rbs = await AppMenu().openReportBrokenSite();
      await rbs.isReasonHidden();
      await rbs.isSendButtonEnabled();
      await clickSendAndCheckPing(rbs);
      await rbs.clickOkay();

      ensureReasonOptional();
      rbs = await AppMenu().openReportBrokenSite();
      await rbs.isReasonOptional();
      await rbs.isSendButtonEnabled();
      await clickSendAndCheckPing(rbs);
      await rbs.clickOkay();

      rbs = await AppMenu().openReportBrokenSite();
      await rbs.isReasonOptional();
      rbs.chooseReason("slow");
      await rbs.isSendButtonEnabled();
      await clickSendAndCheckPing(rbs, "slow");
      await rbs.clickOkay();

      ensureReasonRequired();
      rbs = await AppMenu().openReportBrokenSite();
      await rbs.isReasonRequired();
      await rbs.isSendButtonEnabled();
      const { reasonDropdownPopup, sendButton } = rbs;
      await clickAndAwait(sendButton, "popupshown", reasonDropdownPopup);
      await rbs.dismissDropdownPopup();
      rbs.chooseReason("media");
      await rbs.isSendButtonEnabled();
      await clickSendAndCheckPing(rbs, "media");
      await rbs.clickOkay();
    }
  );
});
