/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_check_alert_from_blank() {
  await BrowserTestUtils.withNewTab(
    "https://example.com/blank",
    async browser => {
      await SpecialPowers.spawn(browser, [], () => {
        let button = content.document.createElement("button");
        button.addEventListener("click", () => {
          let newWin = content.open(
            "about:blank",
            "",
            "popup,width=600,height=600"
          );
          newWin.alert("Alert from the popup.");
        });
        content.document.body.append(button);
      });
      let newWinPromise = BrowserTestUtils.waitForNewWindow();
      await BrowserTestUtils.synthesizeMouseAtCenter("button", {}, browser);
      let otherWin = await newWinPromise;
      Assert.equal(
        otherWin.gBrowser.currentURI?.spec,
        "about:blank",
        "Should have opened about:blank"
      );
      Assert.equal(
        otherWin.menubar.visible,
        false,
        "Should be a popup window."
      );
      let contentDialogManager = gBrowser
        .getTabDialogBox(gBrowser.selectedBrowser)
        .getContentDialogManager();
      todo_is(contentDialogManager._dialogs.length, 1, "Should have 1 dialog.");
      await BrowserTestUtils.closeWindow(otherWin);
    }
  );
});
