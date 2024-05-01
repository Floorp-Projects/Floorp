/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function persist_sidebar_width() {
  {
    // Make the main test window not count as a browser window any longer,
    // which allows the persitence code to kick in.
    const docEl = document.documentElement;
    const oldWinType = docEl.getAttribute("windowtype");
    docEl.setAttribute("windowtype", "navigator:testrunner");
    registerCleanupFunction(() => {
      docEl.setAttribute("windowtype", oldWinType);
    });
  }

  {
    info("Showing new window and setting sidebar box");
    const win = await BrowserTestUtils.openNewBrowserWindow();
    await win.SidebarUI.show("viewBookmarksSidebar");
    win.document.getElementById("sidebar-box").style.width = "100px";
    await BrowserTestUtils.closeWindow(win);
  }

  {
    info("Showing new window and seeing persisted width");
    const win = await BrowserTestUtils.openNewBrowserWindow();
    await win.SidebarUI.show("viewBookmarksSidebar");
    is(
      win.document.getElementById("sidebar-box").style.width,
      "100px",
      "Width style should be persisted"
    );
    await BrowserTestUtils.closeWindow(win);
  }
});
