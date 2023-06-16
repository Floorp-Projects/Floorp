/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test opening toolboxes against a tab and its popup

const TEST_URL = "data:text/html,test for debugging popups";
const POPUP_URL = "data:text/html,popup";

const POPUP_DEBUG_PREF = "devtools.popups.debug";

add_task(async function () {
  const isPopupDebuggingEnabled = Services.prefs.getBoolPref(POPUP_DEBUG_PREF);

  info("Open a tab and debug it");
  const tab = await addTab(TEST_URL);
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "webconsole",
  });

  info("Open a popup");
  const onTabOpened = once(gBrowser.tabContainer, "TabOpen");
  const onToolboxSwitchedToTab = toolbox.once("switched-host-to-tab");
  await SpecialPowers.spawn(tab.linkedBrowser, [POPUP_URL], url => {
    content.open(url);
  });
  const tabOpenEvent = await onTabOpened;
  const popupTab = tabOpenEvent.target;

  const popupToolbox = await gDevTools.showToolboxForTab(popupTab);
  if (isPopupDebuggingEnabled) {
    ok(
      !popupToolbox,
      "When popup debugging is enabled, the popup should be debugged via the same toolbox as the original tab"
    );
    info("Wait for internal event notifying about the toolbox being moved");
    await onToolboxSwitchedToTab;
    const browserContainer = gBrowser.getBrowserContainer(
      popupTab.linkedBrowser
    );
    const iframe = browserContainer.querySelector(
      ".devtools-toolbox-bottom-iframe"
    );
    ok(iframe, "The original tab's toolbox moved to the popup tab");
  } else {
    ok(popupToolbox, "We were able to spawn a toolbox for the popup");
    info("Close the popup toolbox and its tab");
    await popupToolbox.destroy();
  }

  info("Close the popup tab");
  gBrowser.removeCurrentTab();

  info("Close the original tab toolbox and itself");
  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});
