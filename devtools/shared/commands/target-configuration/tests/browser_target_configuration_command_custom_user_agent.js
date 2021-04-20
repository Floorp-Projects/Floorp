/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test setting custom user agent.
const TEST_DOCUMENT = "target_configuration_test_doc.sjs";
const TEST_URI = URL_ROOT_COM_SSL + TEST_DOCUMENT;

add_task(async function() {
  const tab = await addTab(TEST_URI);

  info("Create commands for the tab");
  const commands = await CommandsFactory.forTab(tab);

  const targetConfigurationCommand = commands.targetConfigurationCommand;
  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const initialUserAgent = await getTopLevelUserAgent();

  info("Update configuration to change user agent");
  const CUSTOM_USER_AGENT = "<MY_BORING_CUSTOM_USER_AGENT>";

  await targetConfigurationCommand.updateConfiguration({
    customUserAgent: CUSTOM_USER_AGENT,
  });

  is(
    await getTopLevelUserAgent(),
    CUSTOM_USER_AGENT,
    "The user agent is properly set on the top level document after updating the configuration"
  );
  is(
    await getIframeUserAgent(),
    CUSTOM_USER_AGENT,
    "The user agent is properly set on the iframe after updating the configuration"
  );

  info("Reload the page");
  let onPageLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    true
  );
  gBrowser.reloadTab(tab);
  await onPageLoaded;

  is(
    await getTopLevelDocumentUserAgentAtStartup(),
    CUSTOM_USER_AGENT,
    "The custom user agent was set in the content page when it loaded after reloading"
  );
  is(
    await getTopLevelUserAgent(),
    CUSTOM_USER_AGENT,
    "The custom user agent is set in the content page after reloading"
  );
  is(
    await getIframeUserAgentAtStartup(),
    CUSTOM_USER_AGENT,
    "The custom user agent was set in the remote iframe when it loaded after reloading"
  );
  is(
    await getIframeUserAgent(),
    CUSTOM_USER_AGENT,
    "The custom user agent is set in the remote iframe after reloading"
  );

  const previousBrowsingContextId = gBrowser.selectedBrowser.browsingContext.id;
  info(
    "Check that navigating to a page that forces the creation of a new browsing context keep the simulation enabled"
  );

  onPageLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, true);
  BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    URL_ROOT_ORG_SSL + TEST_DOCUMENT + "?crossOriginIsolated=true"
  );
  await onPageLoaded;

  isnot(
    gBrowser.selectedBrowser.browsingContext.id,
    previousBrowsingContextId,
    "A new browsing context was created"
  );

  is(
    await getTopLevelDocumentUserAgentAtStartup(),
    CUSTOM_USER_AGENT,
    "The custom user agent was set in the content page when it loaded after navigating to a new browsing context"
  );
  is(
    await getTopLevelUserAgent(),
    CUSTOM_USER_AGENT,
    "The custom user agent is set in the content page after navigating to a new browsing context"
  );
  is(
    await getIframeUserAgentAtStartup(),
    CUSTOM_USER_AGENT,
    "The custom user agent was set in the remote iframe when it loaded after navigating to a new browsing context"
  );
  is(
    await getIframeUserAgent(),
    CUSTOM_USER_AGENT,
    "The custom user agent is set in the remote iframe after navigating to a new browsing context"
  );

  info(
    "Create another commands instance and check that destroying it won't reset the user agent"
  );
  const otherCommands = await CommandsFactory.forTab(tab);
  const otherTargetConfigurationCommand =
    otherCommands.targetConfigurationCommand;
  const otherTargetCommand = otherCommands.targetCommand;
  await otherTargetCommand.startListening();
  // wait for the target to be fully attached to avoid pending connection to the server
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

  is(
    await getTopLevelUserAgent(),
    CUSTOM_USER_AGENT,
    "The custom user agent is still set on the page after destroying another commands instance"
  );

  info(
    "Check that destroying the commands we set the user agent in will reset the user agent"
  );
  targetCommand.destroy();
  await commands.destroy();

  // XXX: This is needed at the moment since Navigator.cpp retrieve the UserAgent from the
  // headers (when there's no custom user agent). And here, since we reloaded the page once
  // we set the custom user agent, the header was set accordingly and still holds the custom
  // user agent value. This should be fixed by Bug 1705326.
  is(
    await getTopLevelUserAgent(),
    CUSTOM_USER_AGENT,
    "The custom user agent is still set on the page after destroying the first commands instance. Bug 1705326 will fix that and make it equal to `initialUserAgent`"
  );

  onPageLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, true);
  gBrowser.reloadTab(tab);
  await onPageLoaded;
  is(
    await getTopLevelUserAgent(),
    initialUserAgent,
    "The user agent was reset in the content page after destroying the commands"
  );
  is(
    await getIframeUserAgent(),
    initialUserAgent,
    "The user agent was reset in the remote iframe after destroying the commands"
  );
});

function getUserAgent(browserOrBrowsingContext) {
  return SpecialPowers.spawn(browserOrBrowsingContext, [], () => {
    return content.navigator.userAgent;
  });
}

function getUserAgentAtStartup(browserOrBrowsingContext) {
  return SpecialPowers.spawn(
    browserOrBrowsingContext,
    [],
    () => content.wrappedJSObject.initialUserAgent
  );
}

function getTopLevelUserAgent() {
  return getUserAgent(gBrowser.selectedBrowser);
}

function getTopLevelDocumentUserAgentAtStartup() {
  return getUserAgentAtStartup(gBrowser.selectedBrowser);
}

function getIframeBrowsingContext() {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.document.querySelector("iframe").browsingContext
  );
}

async function getIframeUserAgent() {
  const iframeBC = await getIframeBrowsingContext();
  return getUserAgent(iframeBC);
}

async function getIframeUserAgentAtStartup() {
  const iframeBC = await getIframeBrowsingContext();
  return getUserAgentAtStartup(iframeBC);
}
