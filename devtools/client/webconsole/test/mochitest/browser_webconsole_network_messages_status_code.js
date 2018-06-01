/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_FILE = "test-network-request.html";
const TEST_PATH = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/mochitest/";
const TEST_URI = TEST_PATH + TEST_FILE;

const NET_PREF = "devtools.webconsole.filter.net";
const XHR_PREF = "devtools.webconsole.filter.netxhr";
const { l10n } = require("devtools/client/webconsole/utils/messages");
const LEARN_MORE_URI = "https://developer.mozilla.org/docs/Web/HTTP/Status/200" + STATUS_CODES_GA_PARAMS;

pushPref(NET_PREF, true);
pushPref(XHR_PREF, true);

add_task(async function task() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const currentTab = gBrowser.selectedTab;
  const target = TargetFactory.forTab(currentTab);
  const toolbox = gDevTools.getToolbox(target);
  const {ui} = toolbox.getCurrentPanel().hud;
  const onNetworkMessageUpdate = ui.jsterm.hud.once("network-message-updated");

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

  ok(statusCodeNode.title, l10n.getStr("webConsoleMoreInfoLabel"));
  const {
    middleMouseEvent,
    ctrlOrCmdKeyMouseEvent,
    rightClickMouseEvent,
    rightClickCtrlOrCmdKeyMouseEvent,
  } = getMouseEvents();

  const testCases = [
    { clickEvent: middleMouseEvent, link: LEARN_MORE_URI, where: "tabshifted" },
    { clickEvent: null, link: LEARN_MORE_URI, where: "tab" },
    { clickEvent: ctrlOrCmdKeyMouseEvent, link: LEARN_MORE_URI, where: "tabshifted" },
    { clickEvent: rightClickMouseEvent, link: null, where: null },
    { clickEvent: rightClickCtrlOrCmdKeyMouseEvent, link: null, where: null }
  ];

  for (const testCase of testCases) {
    const { clickEvent } = testCase;
    const onConsoleMenuOpened = [
      rightClickMouseEvent,
      rightClickCtrlOrCmdKeyMouseEvent
    ].includes(clickEvent) ? hud.ui.consoleOutput.once("menu-open") : null;

    const { link, where } = await simulateLinkClick(statusCodeNode, testCase.clickEvent);
    is(link, testCase.link, `Clicking the provided link opens ${link}`);
    is(where, testCase.where, `Link opened in correct tab`);

    if (onConsoleMenuOpened) {
      info("Check if context menu is opened on right clicking the status-code");
      await onConsoleMenuOpened;
    }
  }
});

function getMouseEvents() {
  const isOSX = Services.appinfo.OS == "Darwin";

  const middleMouseEvent = new MouseEvent("click", {
    bubbles: true,
    button: 1,
    view: window
  });
  const ctrlOrCmdKeyMouseEvent = new MouseEvent("click", {
    bubbles: true,
    [isOSX ? "metaKey" : "ctrlKey"]: true,
    view: window
  });
  const rightClickMouseEvent = new MouseEvent("contextmenu", {
    bubbles: true,
    button: 2,
    view: window
  });
  const rightClickCtrlOrCmdKeyMouseEvent = new MouseEvent("contextmenu", {
    bubbles: true,
    button: 2,
    [isOSX ? "metaKey" : "ctrlKey"]: true,
    view: window
  });

  return {
    middleMouseEvent,
    ctrlOrCmdKeyMouseEvent,
    rightClickMouseEvent,
    rightClickCtrlOrCmdKeyMouseEvent,
  };
}
