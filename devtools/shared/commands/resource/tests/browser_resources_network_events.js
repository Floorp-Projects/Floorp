/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around NETWORK_EVENT

// We are borrowing tests from the netmonitor frontend
const NETMONITOR_TEST_FOLDER =
  "https://example.com/browser/devtools/client/netmonitor/test/";
const CSP_URL = `${NETMONITOR_TEST_FOLDER}html_csp-test-page.html`;
const JS_CSP_URL = `${NETMONITOR_TEST_FOLDER}js_websocket-worker-test.js`;
const CSS_CSP_URL = `${NETMONITOR_TEST_FOLDER}internal-loaded.css`;

const CSP_BLOCKED_REASON_CODE = 4000;

add_task(async function() {
  info(`Tests for NETWORK_EVENT resources from the content process`);

  const allResourcesOnAvailable = [];
  const allResourcesOnUpdate = [];

  const tab = await addTab(CSP_URL);
  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();
  const { resourceCommand } = commands;

  let onAvailable = () => {};
  let onUpdated = () => {};

  await new Promise(resolve => {
    onAvailable = resources => {
      for (const resource of resources) {
        allResourcesOnAvailable.push(resource);
      }
    };
    onUpdated = updates => {
      for (const { resource } of updates) {
        allResourcesOnUpdate.push(resource);
      }
      // Make sure we get 3 updates for the 3 requests sent
      if (allResourcesOnUpdate.length == 3) {
        resolve();
      }
    };

    resourceCommand
      .watchResources([resourceCommand.TYPES.NETWORK_EVENT], {
        onAvailable,
        onUpdated,
      })
      .then(() => reloadBrowser());
  });

  is(
    allResourcesOnAvailable.length,
    3,
    "Got three network events fired on available"
  );
  is(
    allResourcesOnUpdate.length,
    3,
    "Got three network events fired on update"
  );

  // Find the page's request
  const availablePageResource = allResourcesOnAvailable.find(
    resource => resource.url === CSP_URL
  );
  is(
    availablePageResource.resourceType,
    resourceCommand.TYPES.NETWORK_EVENT,
    "This is a network event resource"
  );
  is(
    availablePageResource.isNavigationRequest,
    true,
    "The page request is correctly flaged as a navigation request"
  );

  // Find the Blocked CSP JS resource
  const availableJSResource = allResourcesOnAvailable.find(
    resource => resource.url === JS_CSP_URL
  );
  const updateJSResource = allResourcesOnUpdate.find(
    update => update.url === JS_CSP_URL
  );

  // Assert the data for the CSP blocked JS script file
  is(
    availableJSResource.resourceType,
    resourceCommand.TYPES.NETWORK_EVENT,
    "This is a network event resource"
  );
  is(
    availableJSResource.blockedReason,
    CSP_BLOCKED_REASON_CODE,
    "The js resource is blocked by CSP"
  );

  is(
    updateJSResource.resourceType,
    resourceCommand.TYPES.NETWORK_EVENT,
    "This is a network event resource"
  );
  is(
    updateJSResource.blockedReason,
    CSP_BLOCKED_REASON_CODE,
    "The js resource is blocked by CSP"
  );

  // Find the Blocked CSP CSS resource
  const availableCSSResource = allResourcesOnAvailable.find(
    resource => resource.url === CSS_CSP_URL
  );

  const updateCSSResource = allResourcesOnUpdate.find(
    update => update.url === CSS_CSP_URL
  );

  // Assert the data for the CSP blocked CSS file
  is(
    availableCSSResource.resourceType,
    resourceCommand.TYPES.NETWORK_EVENT,
    "This is a network event resource"
  );
  is(
    availableCSSResource.blockedReason,
    CSP_BLOCKED_REASON_CODE,
    "The css resource is blocked by CSP"
  );

  is(
    updateCSSResource.resourceType,
    resourceCommand.TYPES.NETWORK_EVENT,
    "This is a network event resource"
  );
  is(
    updateCSSResource.blockedReason,
    CSP_BLOCKED_REASON_CODE,
    "The css resource is blocked by CSP"
  );

  resourceCommand.unwatchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    onAvailable,
    onUpdated,
  });

  await commands.destroy();

  BrowserTestUtils.removeTab(tab);
});
