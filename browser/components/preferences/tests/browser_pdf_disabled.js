/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that pdf always appears in the applications list even
// both a customized handler doesn't exist and when the internal viewer is
// not enabled.
add_task(async function pdfIsAlwaysPresent() {
  // Try again with the pdf viewer enabled and disabled.
  for (let test of ["enabled", "disabled"]) {
    await SpecialPowers.pushPrefEnv({
      set: [["pdfjs.disabled", test == "disabled"]],
    });

    await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });

    let win = gBrowser.selectedBrowser.contentWindow;

    let container = win.document.getElementById("handlersView");

    // First, find the PDF item.
    let pdfItem = container.querySelector(
      "richlistitem[type='application/pdf']"
    );
    Assert.ok(pdfItem, "pdfItem is present in handlersView when " + test);
    if (pdfItem) {
      pdfItem.scrollIntoView({ block: "center" });
      pdfItem.closest("richlistbox").selectItem(pdfItem);

      // Open its menu
      let list = pdfItem.querySelector(".actionsMenu");
      let popup = list.menupopup;
      let popupShown = BrowserTestUtils.waitForEvent(popup, "popupshown");
      EventUtils.synthesizeMouseAtCenter(list, {}, win);
      await popupShown;

      let handleInternallyItem = list.querySelector(
        `menuitem[action='${Ci.nsIHandlerInfo.handleInternally}']`
      );

      is(
        test == "enabled",
        !!handleInternallyItem,
        "handle internally is present when " + test
      );
    }

    gBrowser.removeCurrentTab();
  }
});
