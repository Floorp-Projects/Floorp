/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around NETWORK_EVENT when navigating

const TEST_URI = `${URL_ROOT_SSL}network_document_navigation.html`;
const JS_URI = TEST_URI.replace(
  "network_document_navigation.html",
  "network_navigation.js"
);

add_task(async () => {
  const tab = await addTab(TEST_URI);
  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();
  const { resourceCommand } = commands;

  const receivedResources = [];
  const onAvailable = resources => {
    for (const resource of resources) {
      is(
        resource.resourceType,
        resourceCommand.TYPES.NETWORK_EVENT,
        "Received a network event resource"
      );
      receivedResources.push(resource);
    }
  };
  const onUpdated = updates => {
    for (const { resource } of updates) {
      is(
        resource.resourceType,
        resourceCommand.TYPES.NETWORK_EVENT,
        "Received a network update event resource"
      );
    }
  };

  await resourceCommand.watchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    ignoreExistingResources: true,
    onAvailable,
    onUpdated,
  });

  await reloadBrowser();

  await waitFor(() => receivedResources.length == 2);

  const navigationRequest = receivedResources[0];
  is(
    navigationRequest.url,
    TEST_URI,
    "The first resource is for the navigation request"
  );

  const jsRequest = receivedResources[1];
  is(jsRequest.url, JS_URI, "The second resource is for the javascript file");

  async function getResponseContent(networkEvent) {
    const packet = {
      to: networkEvent.actor,
      type: "getResponseContent",
    };
    const response = await commands.client.request(packet);
    return response.content.text;
  }

  const HTML_CONTENT = await (await fetch(TEST_URI)).text();
  const JS_CONTENT = await (await fetch(JS_URI)).text();

  const htmlContent = await getResponseContent(navigationRequest);
  is(htmlContent, HTML_CONTENT);
  const jsContent = await getResponseContent(jsRequest);
  is(jsContent, JS_CONTENT);

  await reloadBrowser();

  await waitFor(() => receivedResources.length == 4);

  try {
    await getResponseContent(navigationRequest);
    ok(false, "Shouldn't work");
  } catch (e) {
    is(
      e.error,
      "noSuchActor",
      "Without persist, we can't fetch previous document network data"
    );
  }

  try {
    await getResponseContent(jsRequest);
    ok(false, "Shouldn't work");
  } catch (e) {
    is(
      e.error,
      "noSuchActor",
      "Without persist, we can't fetch previous document network data"
    );
  }

  const navigationRequest2 = receivedResources[2];
  const jsRequest2 = receivedResources[3];
  info("But we can fetch data for the last/new document");
  const htmlContent2 = await getResponseContent(navigationRequest2);
  is(htmlContent2, HTML_CONTENT);
  const jsContent2 = await getResponseContent(jsRequest2);
  is(jsContent2, JS_CONTENT);

  info("Enable persist");
  const networkParentFront =
    await commands.watcherFront.getNetworkParentActor();
  await networkParentFront.setPersist(true);

  await reloadBrowser();

  await waitFor(() => receivedResources.length == 6);

  info("With persist, we can fetch previous document network data");
  const htmlContent3 = await getResponseContent(navigationRequest2);
  is(htmlContent3, HTML_CONTENT);
  const jsContent3 = await getResponseContent(jsRequest2);
  is(jsContent3, JS_CONTENT);

  await resourceCommand.unwatchResources(
    [resourceCommand.TYPES.NETWORK_EVENT],
    {
      onAvailable,
      onUpdated,
    }
  );

  await commands.destroy();
  BrowserTestUtils.removeTab(tab);
});
