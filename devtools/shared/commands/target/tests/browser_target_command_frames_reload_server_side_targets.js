/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the framework handles reloading a document with multiple remote frames (See Bug 1724909).

const REMOTE_ORIGIN = "https://example.com/";
const REMOTE_IFRAME_URL_1 =
  REMOTE_ORIGIN + "/document-builder.sjs?html=first_remote_iframe";
const REMOTE_IFRAME_URL_2 =
  REMOTE_ORIGIN + "/document-builder.sjs?html=second_remote_iframe";
const TEST_URL =
  "https://example.org/document-builder.sjs?html=org" +
  `<iframe src=${REMOTE_IFRAME_URL_1}></iframe>` +
  `<iframe src=${REMOTE_IFRAME_URL_2}></iframe>`;

add_task(async function() {
  // Turn on server side targets
  await pushPref("devtools.target-switching.server.enabled", true);

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
  await targetCommand.watchTargets([TYPES.FRAME], onAvailable, onDestroyed);

  await waitFor(() => targets.length === 3);
  ok(
    true,
    "We are notified about the top-level document and the 2 remote iframes"
  );

  info("Reload the page");
  // When a new target will be created, we need to wait until it's fully processed
  // to avoid pending promises.
  const onNewTargetProcessed = targetCommand.once("processed-available-target");
  gBrowser.reloadTab(tab);
  await onNewTargetProcessed;

  await waitFor(() => targets.length === 6 && destroyedTargets.length === 3);

  // Get the previous targets in a dedicated array and remove them from `targets`
  const previousTargets = targets.splice(0, 3);
  ok(
    previousTargets.every(targetFront => targetFront.isDestroyed()),
    "The previous targets are all destroyed"
  );
  ok(
    targets.every(targetFront => !targetFront.isDestroyed()),
    "The new targets are not destroyed"
  );

  info("Reload one of the iframe");
  SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    const iframeEl = content.document.querySelector("iframe");
    SpecialPowers.spawn(iframeEl.browsingContext, [], () => {
      content.document.location.reload();
    });
  });
  await waitFor(
    () =>
      targets.length + previousTargets.length === 7 &&
      destroyedTargets.length === 4
  );
  const iframeTarget = targets.find(t => t === destroyedTargets.at(-1));
  ok(iframeTarget, "Got the iframe target that got destroyed");
  for (const target of targets) {
    if (target == iframeTarget) {
      ok(
        target.isDestroyed(),
        "The iframe target we navigated from is destroyed"
      );
    } else {
      ok(
        !target.isDestroyed(),
        `Target ${target.actorID}|${target.url} isn't destroyed`
      );
    }
  }

  targetCommand.unwatchTargets([TYPES.FRAME], onAvailable, onDestroyed);
  targetCommand.destroy();
  BrowserTestUtils.removeTab(tab);
  await commands.destroy();
});
