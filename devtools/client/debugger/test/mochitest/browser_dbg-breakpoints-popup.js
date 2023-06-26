/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Verify that we hit breakpoints on popups

"use strict";

const TEST_URI = "https://example.org/document-builder.sjs?html=main page";
const POPUP_URL = `https://example.com/document-builder.sjs?html=${escape(`popup for breakpoints
  <script>
    var paused = true;
    console.log('popup');
    paused = false;
  </script>
`)}`;
const POPUP_DEBUGGER_STATEMENT_URL = `https://example.com/document-builder.sjs?html=${escape(`popup with debugger;
  <script>
    var paused = true;
    debugger;
    paused = false;
  </script>
`)}`;

function isPopupPaused(popupBrowsingContext) {
  return SpecialPowers.spawn(popupBrowsingContext, [], function (url) {
    return content.wrappedJSObject.paused;
  });
}

async function openPopup(popupUrl, browser = gBrowser.selectedBrowser) {
  const onPopupTabSelected = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabSelect"
  );
  const popupBrowsingContext = await SpecialPowers.spawn(
    browser,
    [popupUrl],
    function (url) {
      const popup = content.open(url);
      return popup.browsingContext;
    }
  );
  await onPopupTabSelected;
  is(
    gBrowser.selectedBrowser.browsingContext,
    popupBrowsingContext,
    "The popup is the selected tab"
  );
  return popupBrowsingContext;
}

async function closePopup(browsingContext) {
  const onPreviousTabSelected = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabSelect"
  );
  await SpecialPowers.spawn(browsingContext, [], function () {
    content.close();
  });
  await onPreviousTabSelected;
}

add_task(async function testPausedByBreakpoint() {
  await pushPref("devtools.popups.debug", true);

  info("Test breakpoints set in popup scripts");
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URI);

  info("Open the popup in order to be able to set a breakpoint");
  const firstPopupBrowsingContext = await openPopup(POPUP_URL);

  await waitForSource(dbg, POPUP_URL);
  const source = findSource(dbg, POPUP_URL);

  await selectSource(dbg, source);
  await addBreakpoint(dbg, source, 4);

  info("Now close and reopen the popup");
  await closePopup(firstPopupBrowsingContext);

  info("Re-open the popup");
  const popupBrowsingContext = await openPopup(POPUP_URL);
  await waitForPaused(dbg);
  is(
    await isPopupPaused(popupBrowsingContext),
    true,
    "The popup is really paused"
  );

  await waitForSource(dbg, POPUP_URL);
  assertPausedAtSourceAndLine(dbg, source.id, 4);

  await resume(dbg);
  is(
    await isPopupPaused(popupBrowsingContext),
    false,
    "The popup resumed its execution"
  );
});

add_task(async function testPausedByDebuggerStatement() {
  info("Test debugger statements in popup scripts");
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URI);

  info("Open a popup with a debugger statement");
  const popupBrowsingContext = await openPopup(POPUP_DEBUGGER_STATEMENT_URL);
  await waitForPaused(dbg);
  is(
    await isPopupPaused(popupBrowsingContext),
    true,
    "The popup is really paused"
  );

  const source = findSource(dbg, POPUP_DEBUGGER_STATEMENT_URL);
  assertPausedAtSourceAndLine(dbg, source.id, 4);

  await resume(dbg);
  is(
    await isPopupPaused(popupBrowsingContext),
    false,
    "The popup resumed its execution"
  );
});

add_task(async function testPausedInTwoPopups() {
  info("Test being paused in two popup at the same time");
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URI);

  info("Open the popup in order to be able to set a breakpoint");
  const browser = gBrowser.selectedBrowser;
  const popupBrowsingContext = await openPopup(POPUP_URL);

  await waitForSource(dbg, POPUP_URL);
  const source = findSource(dbg, POPUP_URL);

  await selectSource(dbg, source);
  await addBreakpoint(dbg, source, 4);

  info("Now close and reopen the popup");
  await closePopup(popupBrowsingContext);

  info("Open a first popup which will hit the breakpoint");
  const firstPopupBrowsingContext = await openPopup(POPUP_URL);
  await waitForPaused(dbg);
  const { targetCommand } = dbg.commands;
  const firstTarget = targetCommand
    .getAllTargets([targetCommand.TYPES.FRAME])
    .find(targetFront => targetFront.url == POPUP_URL);
  is(
    firstTarget.browsingContextID,
    firstPopupBrowsingContext.id,
    "The popup target matches the popup BrowsingContext"
  );
  const firstThread = (await firstTarget.getFront("thread")).actorID;
  is(
    dbg.selectors.getCurrentThread(),
    firstThread,
    "The popup thread is automatically selected on pause"
  );
  is(
    await isPopupPaused(firstPopupBrowsingContext),
    true,
    "The first popup is really paused"
  );

  info("Open a second popup which will also hit the breakpoint");
  let onAvailable;
  const onNewTarget = new Promise(resolve => {
    onAvailable = ({ targetFront }) => {
      if (
        targetFront.url == POPUP_URL &&
        targetFront.browsingContextID != firstPopupBrowsingContext.id
      ) {
        targetCommand.unwatchTargets({
          types: [targetCommand.TYPES.FRAME],
          onAvailable,
        });
        resolve(targetFront);
      }
    };
  });
  await targetCommand.watchTargets({
    types: [targetCommand.TYPES.FRAME],
    onAvailable,
  });
  const secondPopupBrowsingContext = await openPopup(POPUP_URL, browser);
  info("Wait for second popup's target");
  const popupTarget = await onNewTarget;
  is(
    popupTarget.browsingContextID,
    secondPopupBrowsingContext.id,
    "The new target matches the popup WindowGlobal"
  );
  const secondThread = (await popupTarget.getFront("thread")).actorID;
  await waitForPausedThread(dbg, secondThread);
  is(
    dbg.selectors.getCurrentThread(),
    secondThread,
    "The second popup thread is automatically selected on pause"
  );
  is(
    await isPopupPaused(secondPopupBrowsingContext),
    true,
    "The second popup is really paused"
  );

  info("Resume the execution of the second popup");
  await resume(dbg);
  is(
    await isPopupPaused(secondPopupBrowsingContext),
    false,
    "The second popup resumed its execution"
  );
  is(
    await isPopupPaused(firstPopupBrowsingContext),
    true,
    "The first popup is still paused"
  );

  info("Resume the execution of the first popup");
  await dbg.actions.selectThread(firstThread);
  await resume(dbg);
  is(
    await isPopupPaused(firstPopupBrowsingContext),
    false,
    "The first popup resumed its execution"
  );
});

add_task(async function testClosingOriginalTab() {
  info(
    "Test closing the toolbox on the original tab while the popup is kept open"
  );
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URI);
  await dbg.toolbox.selectTool("webconsole");

  info("Open a popup");
  const originalTab = gBrowser.selectedTab;
  await openPopup("about:blank");
  await wait(1000);
  const popupTab = gBrowser.selectedTab;
  gBrowser.selectedTab = originalTab;
  info("Close the toolbox from the original tab");
  await dbg.toolbox.closeToolbox();
  await wait(1000);
  info("Re-select the popup");
  gBrowser.selectedTab = popupTab;
  await wait(1000);
});
