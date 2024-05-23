/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_pip_label_changes_tab() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  let pipTab = newWin.document.querySelector(".tabbrowser-tab[selected]");
  pipTab.setAttribute("pictureinpicture", true);

  let pipLabel = pipTab.querySelector(".tab-icon-sound-pip-label");

  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let selectedTab = newWin.document.querySelector(
      ".tabbrowser-tab[selected]"
    );
    Assert.ok(
      selectedTab != pipTab,
      "Picture in picture tab is not selected tab"
    );

    selectedTab = await BrowserTestUtils.switchTab(newWin.gBrowser, () =>
      pipLabel.click()
    );
    Assert.ok(selectedTab == pipTab, "Picture in picture tab is selected tab");
  });

  await BrowserTestUtils.closeWindow(newWin);
});
