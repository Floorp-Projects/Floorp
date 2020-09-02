/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around STYLESHEET.

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const STYLE_TEST_URL = URL_ROOT_SSL + "style_document.html";

const EXISTING_RESOURCES = [
  {
    styleText: "body { color: lime; }",
    href: null,
    nodeHref:
      "https://example.com/browser/devtools/shared/resources/tests/style_document.html",
    isNew: false,
  },
  {
    styleText: "body { margin: 1px; }",
    href:
      "https://example.com/browser/devtools/shared/resources/tests/style_document.css",
    nodeHref:
      "https://example.com/browser/devtools/shared/resources/tests/style_document.html",
    isNew: false,
  },
  {
    styleText: "body { background-color: pink; }",
    href: null,
    nodeHref:
      "https://example.org/browser/devtools/shared/resources/tests/style_iframe.html",
    isNew: false,
  },
  {
    styleText: "body { padding: 1px; }",
    href:
      "https://example.org/browser/devtools/shared/resources/tests/style_iframe.css",
    nodeHref:
      "https://example.org/browser/devtools/shared/resources/tests/style_iframe.html",
    isNew: false,
  },
];

const ADDITIONAL_RESOURCE = {
  styleText: "body { font-size: 10px; }",
  href: null,
  nodeHref:
    "https://example.com/browser/devtools/shared/resources/tests/style_document.html",
  isNew: false,
};

const ADDITIONAL_FROM_ACTOR_RESOURCE = {
  styleText: "body { font-size: 10px; }",
  href: null,
  nodeHref:
    "https://example.com/browser/devtools/shared/resources/tests/style_document.html",
  isNew: true,
};

add_task(async function() {
  const tab = await addTab(STYLE_TEST_URL);

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  info("Check whether ResourceWatcher gets existing stylesheet");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.STYLESHEET], {
    onAvailable: ({ resource }) => availableResources.push(resource),
  });

  is(
    availableResources.length,
    EXISTING_RESOURCES.length,
    "Length of existing resources is correct"
  );
  for (let i = 0; i < availableResources.length; i++) {
    const availableResource = availableResources[i];
    // We can not expect the resources to always be forwarded in the same order.
    // See intermittent Bug 1655016.
    const expectedResource = findMatchingExpectedResource(availableResource);
    ok(expectedResource, "Found a matching expected resource for the resource");
    await assertResource(availableResource, expectedResource);
  }

  info("Check whether ResourceWatcher gets additonal stylesheet");
  await ContentTask.spawn(
    tab.linkedBrowser,
    ADDITIONAL_RESOURCE.styleText,
    text => {
      const document = content.document;
      const stylesheet = document.createElement("style");
      stylesheet.textContent = text;
      document.body.appendChild(stylesheet);
    }
  );
  await waitUntil(
    () => availableResources.length === EXISTING_RESOURCES.length + 1
  );
  await assertResource(
    availableResources[availableResources.length - 1],
    ADDITIONAL_RESOURCE
  );

  info(
    "Check whether ResourceWatcher gets additonal stylesheet which is added by DevTool"
  );
  const styleSheetsFront = await targetList.targetFront.getFront("stylesheets");
  await styleSheetsFront.addStyleSheet(
    ADDITIONAL_FROM_ACTOR_RESOURCE.styleText
  );
  await waitUntil(
    () => availableResources.length === EXISTING_RESOURCES.length + 2
  );
  await assertResource(
    availableResources[availableResources.length - 1],
    ADDITIONAL_FROM_ACTOR_RESOURCE
  );

  info("Check whether the stylesheet actor is updated correctly or not");
  const firstResource = availableResources[0];
  await firstResource.styleSheet.update("", false);
  const expectedResource = findMatchingExpectedResource(firstResource);
  await assertResource(
    availableResources[0],
    Object.assign(expectedResource, { styleText: "" })
  );

  await targetList.destroy();
  await client.close();
});

function findMatchingExpectedResource(resource) {
  return EXISTING_RESOURCES.find(
    expected =>
      resource.styleSheet.href === expected.href &&
      resource.styleSheet.nodeHref === expected.nodeHref
  );
}

async function assertResource(resource, expected) {
  const { resourceType, styleSheet, isNew } = resource;
  is(
    resourceType,
    ResourceWatcher.TYPES.STYLESHEET,
    "Resource type is correct"
  );
  ok(styleSheet, "Stylesheet object is in the resource");
  const styleText = (await styleSheet.getText()).str.trim();
  is(styleText, expected.styleText, "Style text is correct");
  is(styleSheet.href, expected.href, "href is correct");
  is(styleSheet.nodeHref, expected.nodeHref, "nodeHref is correct");
  is(isNew, expected.isNew, "Flag isNew is correct");
}
