/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test color scheme simulation.
const TEST_DOCUMENT = "doc_media_queries.sjs";
const TEST_URI = URL_ROOT_COM_SSL + TEST_DOCUMENT;

add_task(async function() {
  info("Setup the test page with workers of all types");
  const tab = await addTab(TEST_URI);

  is(
    await topLevelDocumentMatchPrefersDarkColorSchemeMediaAtStartup(),
    false,
    "The dark mode simulation wasn't enabled in the content page when it loaded"
  );
  is(
    await topLevelDocumentMatchPrefersDarkColorSchemeMedia(),
    false,
    "The dark mode simulation isn't enabled in the content page by default"
  );
  is(
    await iframeDocumentMatchPrefersDarkColorSchemeMediaAtStartup(),
    false,
    "The dark mode simulation wasn't enabled in the remote iframe when it loaded"
  );
  is(
    await iframeDocumentMatchPrefersDarkColorSchemeMedia(),
    false,
    "The dark mode simulation isn't enabled in the remote iframe by default"
  );

  info("Create a target list for a tab target");
  const commands = await CommandsFactory.forTab(tab);

  const targetConfigurationCommand = commands.targetConfigurationCommand;
  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  info("Update configuration to enable dark mode simulation");
  await targetConfigurationCommand.updateConfiguration({
    colorSchemeSimulation: "dark",
  });
  is(
    await topLevelDocumentMatchPrefersDarkColorSchemeMedia(),
    true,
    "The dark mode simulation is enabled after updating the configuration"
  );
  is(
    await iframeDocumentMatchPrefersDarkColorSchemeMedia(),
    true,
    "The dark mode simulation is enabled in the remote iframe after updating the configuration"
  );

  info("Reload the page");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  is(
    await topLevelDocumentMatchPrefersDarkColorSchemeMediaAtStartup(),
    true,
    "The dark mode simulation was enabled in the content page when it loaded after reloading"
  );
  is(
    await topLevelDocumentMatchPrefersDarkColorSchemeMedia(),
    true,
    "The dark mode simulation is enabled in the content page after reloading"
  );
  is(
    await iframeDocumentMatchPrefersDarkColorSchemeMediaAtStartup(),
    true,
    "The dark mode simulation was enabled in the remote iframe when it loaded after reloading"
  );
  is(
    await iframeDocumentMatchPrefersDarkColorSchemeMedia(),
    true,
    "The dark mode simulation is enabled in the remote iframe after reloading"
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
    await topLevelDocumentMatchPrefersDarkColorSchemeMediaAtStartup(),
    true,
    "The dark mode simulation was enabled in the content page when it loaded after navigating to a new browsing context"
  );
  is(
    await topLevelDocumentMatchPrefersDarkColorSchemeMedia(),
    true,
    "The dark mode simulation is enabled in the content page after navigating to a new browsing context"
  );
  is(
    await iframeDocumentMatchPrefersDarkColorSchemeMediaAtStartup(),
    true,
    "The dark mode simulation was enabled in the remote iframe when it loaded after navigating to a new browsing context"
  );
  is(
    await iframeDocumentMatchPrefersDarkColorSchemeMedia(),
    true,
    "The dark mode simulation is enabled in the remote iframe after navigating to a new browsing context"
  );

  targetCommand.destroy();
  await commands.destroy();

  is(
    await topLevelDocumentMatchPrefersDarkColorSchemeMedia(),
    false,
    "The dark mode simulation is disabled in the content page after destroying the commands"
  );
  is(
    await iframeDocumentMatchPrefersDarkColorSchemeMedia(),
    false,
    "The dark mode simulation is disabled in the remote iframe after destroying the commands"
  );
});

function matchPrefersDarkColorSchemeMedia(browserOrBrowsingContext) {
  return SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => content.matchMedia("(prefers-color-scheme: dark)").matches
  );
}

function matchPrefersDarkColorSchemeMediaAtStartup(browserOrBrowsingContext) {
  return SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => content.wrappedJSObject.initialMatchesPrefersDarkColorScheme
  );
}

function topLevelDocumentMatchPrefersDarkColorSchemeMedia() {
  return matchPrefersDarkColorSchemeMedia(gBrowser.selectedBrowser);
}

function topLevelDocumentMatchPrefersDarkColorSchemeMediaAtStartup() {
  return matchPrefersDarkColorSchemeMediaAtStartup(gBrowser.selectedBrowser);
}

function getIframeBrowsingContext() {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.document.querySelector("iframe").browsingContext
  );
}

async function iframeDocumentMatchPrefersDarkColorSchemeMedia() {
  const iframeBC = await getIframeBrowsingContext();
  return matchPrefersDarkColorSchemeMedia(iframeBC);
}

async function iframeDocumentMatchPrefersDarkColorSchemeMediaAtStartup() {
  const iframeBC = await getIframeBrowsingContext();
  return matchPrefersDarkColorSchemeMediaAtStartup(iframeBC);
}
