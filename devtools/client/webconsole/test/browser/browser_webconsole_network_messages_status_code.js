/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH =
  "http://example.com/browser/devtools/client/webconsole/" + "test/browser/";
const TEST_URI = TEST_PATH + TEST_FILE;

const NET_PREF = "devtools.webconsole.filter.net";
const XHR_PREF = "devtools.webconsole.filter.netxhr";
const { l10n } = require("devtools/client/webconsole/utils/messages");
const LEARN_MORE_URI =
  "https://developer.mozilla.org/docs/Web/HTTP/Status/200" + GA_PARAMS;

pushPref(NET_PREF, true);
pushPref(XHR_PREF, true);

add_task(async function task() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const currentTab = gBrowser.selectedTab;
  const target = await TargetFactory.forTab(currentTab);
  const toolbox = gDevTools.getToolbox(target);
  const { ui } = toolbox.getCurrentPanel().hud;
  const onNetworkMessageUpdate = ui.once("network-message-updated");

  // Fire an XHR POST request.
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.testXhrPost();
  });

  info("XHR executed");
  await onNetworkMessageUpdate;

  const xhrUrl = TEST_PATH + "test-data.json";
  const messageNode = await waitFor(() => findMessage(hud, xhrUrl));
  const statusCodeNode = messageNode.querySelector(".status-code");
  info("Network message found.");

  is(
    statusCodeNode.title,
    l10n.getStr("webConsoleMoreInfoLabel"),
    "Status code has the expected tooltip"
  );

  const {
    rightClickMouseEvent,
    rightClickCtrlOrCmdKeyMouseEvent,
  } = getMouseEvents();

  const testCases = [
    { clickEvent: null, link: LEARN_MORE_URI, where: "tab" },
    { clickEvent: rightClickMouseEvent, link: null, where: null },
    { clickEvent: rightClickCtrlOrCmdKeyMouseEvent, link: null, where: null },
  ];

  for (const testCase of testCases) {
    info("Test case");
    const { clickEvent } = testCase;
    const onConsoleMenuOpened = [
      rightClickMouseEvent,
      rightClickCtrlOrCmdKeyMouseEvent,
    ].includes(clickEvent)
      ? hud.ui.wrapper.once("menu-open")
      : null;

    const { link, where } = await simulateLinkClick(
      statusCodeNode,
      testCase.clickEvent
    );
    is(link, testCase.link, `Clicking the provided link opens ${link}`);
    is(where, testCase.where, `Link opened in correct tab`);

    if (onConsoleMenuOpened) {
      info("Check if context menu is opened on right clicking the status-code");
      await onConsoleMenuOpened;
      ok(true, "Console menu is opened");
    }
  }
});

function getMouseEvents() {
  const isOSX = Services.appinfo.OS == "Darwin";

  const rightClickMouseEvent = new MouseEvent("contextmenu", {
    bubbles: true,
    button: 2,
    view: window,
  });
  const rightClickCtrlOrCmdKeyMouseEvent = new MouseEvent("contextmenu", {
    bubbles: true,
    button: 2,
    [isOSX ? "metaKey" : "ctrlKey"]: true,
    view: window,
  });

  return {
    rightClickMouseEvent,
    rightClickCtrlOrCmdKeyMouseEvent,
  };
}
