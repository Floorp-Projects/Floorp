/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we create targets for popups

const TEST_URL = "https://example.org/document-builder.sjs?html=main page";
const POPUP_URL = "https://example.com/document-builder.sjs?html=popup";
const POPUP_SECOND_URL =
  "https://example.com/document-builder.sjs?html=popup-navigated";

add_task(async function () {
  await pushPref("devtools.popups.debug", true);
  // We expect to create a target for a same-process iframe
  // in the test against window.open to load a document in an iframe.
  await pushPref("devtools.every-frame-target.enabled", true);

  // Create a TargetCommand for a given test tab
  const tab = await addTab(TEST_URL);
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  const { TYPES } = targetCommand;

  await targetCommand.startListening();

  // Assert that watchTargets will call the create callback for all existing frames
  const targets = [];
  const destroyedTargets = [];
  const onAvailable = ({ targetFront }) => {
    targets.push(targetFront);
  };
  const onDestroyed = ({ targetFront }) => {
    destroyedTargets.push(targetFront);
  };
  await targetCommand.watchTargets({
    types: [TYPES.FRAME],
    onAvailable,
    onDestroyed,
  });

  is(targets.length, 1, "At first, we only get one target");
  is(
    targets[0],
    targetCommand.targetFront,
    "And this target is the top level one"
  );

  info("Open a popup");
  const firstPopupBrowsingContext = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [POPUP_URL],
    url => {
      const win = content.open(url);
      return win.browsingContext;
    }
  );

  await waitFor(() => targets.length === 2);
  ok(true, "We are notified about the first popup's target");

  is(
    targets[1].browsingContextID,
    firstPopupBrowsingContext.id,
    "the new target is for the popup"
  );
  is(targets[1].url, POPUP_URL, "the new target has the right url");

  info("Navigate the popup to a second location");
  await SpecialPowers.spawn(
    firstPopupBrowsingContext,
    [POPUP_SECOND_URL],
    url => {
      content.location.href = url;
    }
  );

  await waitFor(() => targets.length === 3);
  ok(true, "We are notified about the new location popup's target");

  await waitFor(() => destroyedTargets.length === 1);
  ok(true, "The first popup's target is destroyed");
  is(
    destroyedTargets[0],
    targets[1],
    "The destroyed target is the popup's one"
  );

  is(
    targets[2].browsingContextID,
    firstPopupBrowsingContext.id,
    "the new location target is for the popup"
  );
  is(
    targets[2].url,
    POPUP_SECOND_URL,
    "the new location target has the right url"
  );

  info("Close the popup");
  await SpecialPowers.spawn(firstPopupBrowsingContext, [], () => {
    content.close();
  });

  await waitFor(() => destroyedTargets.length === 2);
  ok(true, "The popup's target is destroyed");
  is(
    destroyedTargets[1],
    targets[2],
    "The destroyed target is the popup's one"
  );

  info("Open a about:blank popup");
  const aboutBlankPopupBrowsingContext = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      const win = content.open("about:blank");
      return win.browsingContext;
    }
  );

  await waitFor(() => targets.length === 4);
  ok(true, "We are notified about the about:blank popup's target");

  is(
    targets[3].browsingContextID,
    aboutBlankPopupBrowsingContext.id,
    "the new target is for the popup"
  );
  is(targets[3].url, "about:blank", "the new target has the right url");

  info("Select the original tab and reload it");
  gBrowser.selectedTab = tab;
  await BrowserTestUtils.reloadTab(tab);

  await waitFor(() => targets.length === 5);
  is(targets[4], targetCommand.targetFront, "We get a new top level target");
  ok(!targets[3].isDestroyed(), "The about:blank popup target is still alive");

  info("Call about:blank popup method to ensure it really is functional");
  await targets[3].logInPage("foo");

  info(
    "Ensure that iframe using window.open to load their document aren't considered as popups"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const iframe = content.document.createElement("iframe");
    iframe.setAttribute("name", "test-iframe");
    content.document.documentElement.appendChild(iframe);
    content.open("data:text/html,iframe", "test-iframe");
  });
  await waitFor(() => targets.length === 6);
  is(
    targets[5].targetForm.isPopup,
    false,
    "The iframe target isn't considered as a popup"
  );

  targetCommand.unwatchTargets({
    types: [TYPES.FRAME],
    onAvailable,
    onDestroyed,
  });
  targetCommand.destroy();
  BrowserTestUtils.removeTab(tab);
  await commands.destroy();
});
