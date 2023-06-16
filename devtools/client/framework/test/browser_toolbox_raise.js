/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "data:text/html,test for opening toolbox in different hosts";

var { Toolbox } = require("resource://devtools/client/framework/toolbox.js");

add_task(async function () {
  const tab1 = await addTab(TEST_URL);
  const tab2 = BrowserTestUtils.addTab(gBrowser);

  const toolbox = await gDevTools.showToolboxForTab(tab1);
  await testBottomHost(toolbox, tab1, tab2);

  await testWindowHost(toolbox);

  Services.prefs.setCharPref("devtools.toolbox.host", Toolbox.HostType.BOTTOM);

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});

async function testBottomHost(toolbox, tab1, tab2) {
  // switch to another tab and test toolbox.raise()
  gBrowser.selectedTab = tab2;
  await new Promise(executeSoon);
  is(
    gBrowser.selectedTab,
    tab2,
    "Correct tab is selected before calling raise"
  );

  await toolbox.raise();
  is(
    gBrowser.selectedTab,
    tab1,
    "Correct tab was selected after calling raise"
  );
}

async function testWindowHost(toolbox) {
  await toolbox.switchHost(Toolbox.HostType.WINDOW);

  info("Wait for the toolbox to be focused when switching to window host");
  // We can't wait for the "focus" event on toolbox.win.parent as this document is created while calling switchHost.
  await waitFor(() => {
    return Services.focus.activeWindow == toolbox.topWindow;
  });

  const onBrowserWindowFocused = new Promise(resolve =>
    window.addEventListener("focus", resolve, { once: true, capture: true })
  );

  info("Focusing the browser window");
  window.focus();

  info("Wait for the browser window to be focused");
  await onBrowserWindowFocused;

  // Now raise toolbox.
  await toolbox.raise();
  is(
    Services.focus.activeWindow,
    toolbox.topWindow,
    "the toolbox window is immediately focused after raise resolution"
  );
}
