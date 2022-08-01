/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that tabs running in the parent process can hear about updates
 * to lightweight themes via contentTheme.js.
 *
 * The test loads the History Sidebar document in a tab to avoid having
 * to create a special parent-process page for the LightweightTheme
 * JSWindow actors.
 */
add_task(async function test_in_process_tab() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  const IN_PROCESS_URI = "chrome://browser/content/places/historySidebar.xhtml";
  const SIDEBAR_BGCOLOR = "rgb(255, 0, 0)";
  // contentTheme.js will always convert the sidebar text color to rgba, so
  // we need to compare against that.
  const SIDEBAR_TEXT_COLOR = "rgba(0, 255, 0, 1)";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser: win.gBrowser,
      url: IN_PROCESS_URI,
    },
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [SIDEBAR_BGCOLOR, SIDEBAR_TEXT_COLOR],
        async (bgColor, textColor) => {
          let style = content.document.documentElement.style;
          Assert.notEqual(
            style.getPropertyValue("--lwt-sidebar-background-color"),
            bgColor
          );
          Assert.notEqual(
            style.getPropertyValue("--lwt-sidebar-text-color"),
            textColor
          );
        }
      );

      // Now cobble together a very simple theme that sets the sidebar background
      // and text color.
      let lwtData = {
        theme: {
          sidebar: SIDEBAR_BGCOLOR,
          sidebar_text: SIDEBAR_TEXT_COLOR,
        },
        darkTheme: null,
        window: win.docShell.outerWindowID,
      };

      Services.obs.notifyObservers(lwtData, "lightweight-theme-styling-update");

      await SpecialPowers.spawn(
        browser,
        [SIDEBAR_BGCOLOR, SIDEBAR_TEXT_COLOR],
        async (bgColor, textColor) => {
          let style = content.document.documentElement.style;
          Assert.equal(
            style.getPropertyValue("--lwt-sidebar-background-color"),
            bgColor,
            "The sidebar background text color should have been set by " +
              "contentTheme.js"
          );
          Assert.equal(
            style.getPropertyValue("--lwt-sidebar-text-color"),
            textColor,
            "The sidebar background text color should have been set by " +
              "contentTheme.js"
          );
        }
      );
    }
  );

  // Wait for the end of the CSS transition happening on the navigation
  // toolbar background-color before closing the window, or compositing
  // never stops on Mac. See bug 1781524.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 100));

  await BrowserTestUtils.closeWindow(win);
});
