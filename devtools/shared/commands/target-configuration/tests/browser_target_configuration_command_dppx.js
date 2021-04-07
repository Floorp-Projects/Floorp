/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test device pixel ratio override.
const TEST_DOCUMENT = "doc_media_queries.sjs";
const TEST_URI = URL_ROOT_COM_SSL + TEST_DOCUMENT;

add_task(async function() {
  const tab = await addTab(TEST_URI);

  info("Create commands for the tab");
  const commands = await CommandsFactory.forTab(tab);

  const targetConfigurationCommand = commands.targetConfigurationCommand;
  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const originalDpr = await getTopLevelDocumentDevicePixelRatio();

  info("Update configuration to change device pixel ratio");
  const CUSTOM_DPR = 5.5;

  await targetConfigurationCommand.updateConfiguration({
    overrideDPPX: CUSTOM_DPR,
  });

  is(
    await getTopLevelDocumentDevicePixelRatio(),
    CUSTOM_DPR,
    "The ratio is properly set on the top level document after updating the configuration"
  );
  is(
    await getIframeDocumentDevicePixelRatio(),
    CUSTOM_DPR,
    "The ratio is properly set on the iframe after updating the configuration"
  );

  info("Reload the page");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  is(
    await getTopLevelDocumentDevicePixelRatioAtStartup(),
    CUSTOM_DPR,
    "The custom ratio was set in the content page when it loaded after reloading"
  );
  is(
    await getTopLevelDocumentDevicePixelRatio(),
    CUSTOM_DPR,
    "The custom ratio is set in the content page after reloading"
  );
  is(
    await getIframeDocumentDevicePixelRatioAtStartup(),
    CUSTOM_DPR,
    "The custom ratio was set in the remote iframe when it loaded after reloading"
  );
  is(
    await getIframeDocumentDevicePixelRatio(),
    CUSTOM_DPR,
    "The custom ratio is set in the remote iframe after reloading"
  );

  const previousBrowsingContextId = gBrowser.selectedBrowser.browsingContext.id;
  info(
    "Check that navigating to a page that forces the creation of a new browsing context keep the simulation enabled"
  );

  BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    URL_ROOT_ORG_SSL + TEST_DOCUMENT + "?crossOriginIsolated=true"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  isnot(
    gBrowser.selectedBrowser.browsingContext.id,
    previousBrowsingContextId,
    "A new browsing context was created"
  );

  is(
    await getTopLevelDocumentDevicePixelRatioAtStartup(),
    CUSTOM_DPR,
    "The custom ratio was set in the content page when it loaded after navigating to a new browsing context"
  );
  is(
    await getTopLevelDocumentDevicePixelRatio(),
    CUSTOM_DPR,
    "The custom ratio is set in the content page after navigating to a new browsing context"
  );
  is(
    await getIframeDocumentDevicePixelRatioAtStartup(),
    CUSTOM_DPR,
    "The custom ratio was set in the remote iframe when it loaded after navigating to a new browsing context"
  );
  is(
    await getIframeDocumentDevicePixelRatio(),
    CUSTOM_DPR,
    "The custom ratio is set in the remote iframe after navigating to a new browsing context"
  );

  info(
    "Create another commands instance and check that destroying it won't reset the ratio"
  );
  const otherCommands = await CommandsFactory.forTab(tab);
  const otherTargetConfigurationCommand =
    otherCommands.targetConfigurationCommand;
  const otherTargetCommand = otherCommands.targetCommand;
  await otherTargetCommand.startListening();

  // Let's update the configuration with this commands instance to make sure we hit the TargetConfigurationActor
  await otherTargetConfigurationCommand.updateConfiguration({
    colorSchemeSimulation: "dark",
  });

  otherTargetCommand.destroy();
  await otherCommands.destroy();

  is(
    await getTopLevelDocumentDevicePixelRatio(),
    CUSTOM_DPR,
    "The custom ratio is still set on the page after destroying another commands instance"
  );

  info(
    "Check that destroying the commands we overrode the ratio in will reset the page ratio"
  );
  targetCommand.destroy();
  await commands.destroy();

  is(
    await getTopLevelDocumentDevicePixelRatio(),
    originalDpr,
    "The ratio was reset in the content page after destroying the commands"
  );
  is(
    await getIframeDocumentDevicePixelRatio(),
    originalDpr,
    "The ratio was reset in the remote iframe after destroying the commands"
  );
});

function getDevicePixelRatio(browserOrBrowsingContext) {
  return SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => content.devicePixelRatio
  );
}

function getDevicePixelRatioAtStartup(browserOrBrowsingContext) {
  return SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => content.wrappedJSObject.initialDevicePixelRatio
  );
}

function getTopLevelDocumentDevicePixelRatio() {
  return getDevicePixelRatio(gBrowser.selectedBrowser);
}

function getTopLevelDocumentDevicePixelRatioAtStartup() {
  return getDevicePixelRatioAtStartup(gBrowser.selectedBrowser);
}

function getIframeBrowsingContext() {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.document.querySelector("iframe").browsingContext
  );
}

async function getIframeDocumentDevicePixelRatio() {
  const iframeBC = await getIframeBrowsingContext();
  return getDevicePixelRatio(iframeBC);
}

async function getIframeDocumentDevicePixelRatioAtStartup() {
  const iframeBC = await getIframeBrowsingContext();
  return getDevicePixelRatioAtStartup(iframeBC);
}
