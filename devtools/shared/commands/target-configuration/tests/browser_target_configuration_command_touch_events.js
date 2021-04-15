/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test touch event simulation.
const TEST_DOCUMENT = "doc_media_queries.sjs";
const TEST_URI = URL_ROOT_COM_SSL + TEST_DOCUMENT;

add_task(async function() {
  // Disable click hold and double tap zooming as it might interfere with the test
  await pushPref("ui.click_hold_context_menus", false);
  await pushPref("apz.allow_double_tap_zooming", false);

  const tab = await addTab(TEST_URI);

  info("Create commands for the tab");
  const commands = await CommandsFactory.forTab(tab);

  const targetConfigurationCommand = commands.targetConfigurationCommand;
  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  info("Touch simulation is disabled at the beginning");
  await checkTopLevelDocumentTouchSimulation({ enabled: false });
  await checkIframeTouchSimulation({
    enabled: false,
  });

  info("Enable touch simulation");
  await targetConfigurationCommand.setTouchEventsOverride("enabled");
  await checkTopLevelDocumentTouchSimulation({ enabled: true });
  await checkIframeTouchSimulation({
    enabled: true,
    // touch events are not emitted in remote frame when fission is enabled.
    // This should be removed in Bug 1704028.
    skipTouchEventsCheck: Services.appinfo.fissionAutostart,
  });

  info("Reload the page");
  const onPageReloaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    true
  );
  gBrowser.reloadTab(tab);
  await onPageReloaded;

  is(
    await topLevelDocumentMatchesCoarsePointerAtStartup(),
    true,
    "The touch simulation was enabled in the content page when it loaded after reloading"
  );
  await checkTopLevelDocumentTouchSimulation({ enabled: true });

  is(
    await iframeMatchesCoarsePointerAtStartup(),
    true,
    "The touch simulation was enabled in the iframe when it loaded after reloading"
  );
  await checkIframeTouchSimulation({
    enabled: true,
    // touch events are not emitted in remote frame when fission is enabled.
    // This should be removed in Bug 1704028.
    skipTouchEventsCheck: Services.appinfo.fissionAutostart,
  });

  info(
    "Create another commands instance and check that destroying it won't reset the touch simulation"
  );
  const otherCommands = await CommandsFactory.forTab(tab);
  const otherTargetConfigurationCommand =
    otherCommands.targetConfigurationCommand;
  const otherTargetCommand = otherCommands.targetCommand;

  await otherTargetCommand.startListening();
  // Watch targets so we wait for server communication to settle (e.g. attach calls), as
  // this could cause intermittent failures.
  await otherTargetCommand.watchTargets(
    [otherTargetCommand.TYPES.FRAME],
    () => {}
  );

  // Let's update the configuration with this commands instance to make sure we hit the TargetConfigurationActor
  await otherTargetConfigurationCommand.updateConfiguration({
    colorSchemeSimulation: "dark",
  });

  otherTargetCommand.destroy();
  await otherCommands.destroy();

  await checkTopLevelDocumentTouchSimulation({ enabled: true });
  await checkIframeTouchSimulation({
    enabled: true,
    // touch events are not emitted in remote frame when fission is enabled.
    // This should be removed in Bug 1704028.
    skipTouchEventsCheck: Services.appinfo.fissionAutostart,
  });

  const previousBrowsingContextId = gBrowser.selectedBrowser.browsingContext.id;
  info(
    "Check that navigating to a page that forces the creation of a new browsing context keep the simulation enabled"
  );

  const onBrowserLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    true
  );
  BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    URL_ROOT_ORG_SSL + TEST_DOCUMENT + "?crossOriginIsolated=true"
  );
  await onBrowserLoaded;

  isnot(
    gBrowser.selectedBrowser.browsingContext.id,
    previousBrowsingContextId,
    "A new browsing context was created"
  );

  is(
    await topLevelDocumentMatchesCoarsePointerAtStartup(),
    true,
    "The touch simulation was enabled in the content page when it loaded after navigating to a new browsing context"
  );
  await checkTopLevelDocumentTouchSimulation({
    enabled: true,
    // The touch simulator isn't working after a new browsing context is created.
    // This should be removed in Bug 1704029.
    skipTouchEventsCheck: true,
  });

  is(
    await iframeMatchesCoarsePointerAtStartup(),
    true,
    "The touch simulation was enabled in the iframe when it loaded after navigating to a new browsing context"
  );
  await checkIframeTouchSimulation({
    enabled: true,
    // The touch simulator isn't working after a new browsing context is created,
    // and touch events are not emitted in remote frame when fission is enabled.
    // This can be removed once Bug 1704029 and Bug 1704028 are resolved.
    skipTouchEventsCheck: true,
  });

  info(
    "Check that destroying the commands we enabled the simulation in will disable the simulation"
  );
  targetCommand.destroy();
  await commands.destroy();

  await checkTopLevelDocumentTouchSimulation({ enabled: false });
  await checkIframeTouchSimulation({
    enabled: false,
  });
});

function matchesCoarsePointer(browserOrBrowsingContext) {
  return SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => content.matchMedia("(pointer: coarse)").matches
  );
}

function matchesCoarsePointerAtStartup(browserOrBrowsingContext) {
  return SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => content.wrappedJSObject.initialMatchesCoarsePointer
  );
}

async function isTouchEventEmitted(browserOrBrowsingContext) {
  const onTimeout = wait(1000).then(() => "TIMEOUT");
  const onTouchEvent = SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    async () => {
      content.touchStartController = new content.AbortController();
      const el = content.document.querySelector("button");

      let gotTouchEndEvent = false;

      const promise = new Promise(resolve => {
        el.addEventListener(
          "touchend",
          () => {
            gotTouchEndEvent = true;
            resolve();
          },
          {
            signal: content.touchStartController.signal,
            once: true,
          }
        );
      });

      // For some reason, it might happen that the event is properly registered and transformed
      // in the touch simulator, but not received by the event listener we set up just before.
      // So here let's try to "tap" 3 times to give us more chance to catch the event.
      for (let i = 0; i < 3; i++) {
        if (gotTouchEndEvent) {
          break;
        }

        // Simulate a "tap" with mousedown and then mouseup.
        EventUtils.synthesizeMouseAtCenter(
          el,
          { type: "mousedown", isSynthesized: false },
          content
        );

        await new Promise(res => content.setTimeout(res, 10));
        EventUtils.synthesizeMouseAtCenter(
          el,
          { type: "mouseup", isSynthesized: false },
          content
        );
        await new Promise(res => content.setTimeout(res, 50));
      }

      return promise;
    }
  );

  const result = await Promise.race([onTimeout, onTouchEvent]);

  // Remove the event listener
  await SpecialPowers.spawn(browserOrBrowsingContext, [], () => {
    content.touchStartController.abort();
    delete content.touchStartController;
  });

  return result !== "TIMEOUT";
}

async function checkTopLevelDocumentTouchSimulation({
  enabled,
  skipTouchEventsCheck = false,
}) {
  is(
    await matchesCoarsePointer(gBrowser.selectedBrowser),
    enabled,
    `The touch simulation is ${
      enabled ? "enabled" : "disabled"
    } on the top level document`
  );

  if (skipTouchEventsCheck) {
    return;
  }

  is(
    await isTouchEventEmitted(gBrowser.selectedBrowser),
    enabled,
    `touch events are ${enabled ? "" : "not "}emitted on the top level document`
  );
}

function topLevelDocumentMatchesCoarsePointerAtStartup() {
  return matchesCoarsePointerAtStartup(gBrowser.selectedBrowser);
}

function getIframeBrowsingContext() {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.document.querySelector("iframe").browsingContext
  );
}

async function checkIframeTouchSimulation({
  enabled,
  skipTouchEventsCheck = false,
}) {
  const iframeBC = await getIframeBrowsingContext();
  is(
    await matchesCoarsePointer(iframeBC),
    enabled,
    `The touch simulation is ${enabled ? "enabled" : "disabled"} on the iframe`
  );

  if (skipTouchEventsCheck) {
    return;
  }

  is(
    await isTouchEventEmitted(iframeBC),
    enabled,
    `touch events are ${enabled ? "" : "not "}emitted on the iframe`
  );
}

async function iframeMatchesCoarsePointerAtStartup() {
  const iframeBC = await getIframeBrowsingContext();
  return matchesCoarsePointerAtStartup(iframeBC);
}
