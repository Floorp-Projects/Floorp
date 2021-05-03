/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around STYLESHEET.

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

const STYLE_TEST_URL = URL_ROOT_SSL + "style_document.html";

const EXISTING_RESOURCES = [
  {
    styleText: "body { color: lime; }",
    href: null,
    nodeHref:
      "https://example.com/browser/devtools/shared/resources/tests/style_document.html",
    isNew: false,
    disabled: false,
    ruleCount: 1,
    mediaRules: [],
  },
  {
    styleText: "body { margin: 1px; }",
    href:
      "https://example.com/browser/devtools/shared/resources/tests/style_document.css",
    nodeHref:
      "https://example.com/browser/devtools/shared/resources/tests/style_document.html",
    isNew: false,
    disabled: false,
    ruleCount: 1,
    mediaRules: [],
  },
  {
    styleText: "body { background-color: pink; }",
    href: null,
    nodeHref:
      "https://example.org/browser/devtools/shared/resources/tests/style_iframe.html",
    isNew: false,
    disabled: false,
    ruleCount: 1,
    mediaRules: [],
  },
  {
    styleText: "body { padding: 1px; }",
    href:
      "https://example.org/browser/devtools/shared/resources/tests/style_iframe.css",
    nodeHref:
      "https://example.org/browser/devtools/shared/resources/tests/style_iframe.html",
    isNew: false,
    disabled: false,
    ruleCount: 1,
    mediaRules: [],
  },
];

const ADDITIONAL_RESOURCE = {
  styleText:
    "@media all { body { color: red; } } @media print { body { color: cyan; } } body { font-size: 10px; }",
  href: null,
  nodeHref:
    "https://example.com/browser/devtools/shared/resources/tests/style_document.html",
  isNew: false,
  disabled: false,
  ruleCount: 3,
  mediaRules: [
    {
      conditionText: "all",
      mediaText: "all",
      matches: true,
      line: 1,
      column: 1,
    },
    {
      conditionText: "print",
      mediaText: "print",
      matches: false,
      line: 1,
      column: 37,
    },
  ],
};

const ADDITIONAL_FROM_ACTOR_RESOURCE = {
  styleText: "body { font-size: 10px; }",
  href: null,
  nodeHref:
    "https://example.com/browser/devtools/shared/resources/tests/style_document.html",
  isNew: true,
  disabled: false,
  ruleCount: 1,
  mediaRules: [],
};

add_task(async function() {
  await testResourceAvailableFeature();
  await testResourceUpdateFeature();
  await testNestedResourceUpdateFeature();
});

async function testResourceAvailableFeature() {
  info("Check resource available feature of the ResourceCommand");

  const tab = await addTab(STYLE_TEST_URL);

  const { client, resourceWatcher, targetCommand } = await initResourceCommand(
    tab
  );

  info("Check whether ResourceCommand gets existing stylesheet");
  const availableResources = [];
  await resourceWatcher.watchResources([resourceWatcher.TYPES.STYLESHEET], {
    onAvailable: resources => availableResources.push(...resources),
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

  info("Check whether ResourceCommand gets additonal stylesheet");
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
    "Check whether ResourceCommand gets additonal stylesheet which is added by DevTool"
  );
  const styleSheetsFront = await targetCommand.targetFront.getFront(
    "stylesheets"
  );
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

  targetCommand.destroy();
  await client.close();
}

async function testResourceUpdateFeature() {
  info("Check resource update feature of the ResourceCommand");

  const tab = await addTab(STYLE_TEST_URL);

  const { client, resourceWatcher, targetCommand } = await initResourceCommand(
    tab
  );

  info("Setup the watcher");
  const availableResources = [];
  const updates = [];
  await resourceWatcher.watchResources([resourceWatcher.TYPES.STYLESHEET], {
    onAvailable: resources => availableResources.push(...resources),
    onUpdated: newUpdates => updates.push(...newUpdates),
  });
  is(
    availableResources.length,
    EXISTING_RESOURCES.length,
    "Length of existing resources is correct"
  );
  is(updates.length, 0, "there's no update yet");

  info("Check toggleDisabled function");
  // Retrieve the stylesheet of the top-level target
  const resource = availableResources.find(
    innerResource => innerResource.targetFront.isTopLevel
  );
  const styleSheetsFront = await resource.targetFront.getFront("stylesheets");
  await styleSheetsFront.toggleDisabled(resource.resourceId);
  await waitUntil(() => updates.length === 1);

  // Check the content of the update object.
  assertUpdate(updates[0].update, {
    resourceId: resource.resourceId,
    updateType: "property-change",
  });
  is(
    updates[0].update.resourceUpdates.disabled,
    true,
    "resourceUpdates is correct"
  );

  // Check whether the cached resource is updated correctly.
  is(
    updates[0].resource.disabled,
    true,
    "cached resource is updated correctly"
  );

  // Check whether the actual stylesheet is updated correctly.
  const styleSheetDisabled = await ContentTask.spawn(
    tab.linkedBrowser,
    null,
    () => {
      const document = content.document;
      const stylesheet = document.styleSheets[0];
      return stylesheet.disabled;
    }
  );
  is(styleSheetDisabled, true, "actual stylesheet was updated correctly");

  info("Check update function");
  const expectedMediaRules = [
    {
      conditionText: "screen",
      mediaText: "screen",
      matches: true,
    },
    {
      conditionText: "print",
      mediaText: "print",
      matches: false,
    },
  ];

  const updateCause = "updated-by-test";
  await styleSheetsFront.update(
    resource.resourceId,
    "@media screen { color: red; } @media print { color: green; } body { color: cyan; }",
    false,
    updateCause
  );
  await waitUntil(() => updates.length === 4);

  assertUpdate(updates[1].update, {
    resourceId: resource.resourceId,
    updateType: "property-change",
  });
  is(
    updates[1].update.resourceUpdates.ruleCount,
    3,
    "resourceUpdates is correct"
  );
  is(updates[1].resource.ruleCount, 3, "cached resource is updated correctly");

  assertUpdate(updates[2].update, {
    resourceId: resource.resourceId,
    updateType: "style-applied",
    event: {
      cause: updateCause,
    },
  });
  is(
    updates[2].update.resourceUpdates,
    undefined,
    "resourceUpdates is correct"
  );

  assertUpdate(updates[3].update, {
    resourceId: resource.resourceId,
    updateType: "media-rules-changed",
  });
  assertMediaRules(
    updates[3].update.resourceUpdates.mediaRules,
    expectedMediaRules
  );

  // Check the actual page.
  const styleSheetResult = await getStyleSheetResult(tab);

  is(
    styleSheetResult.ruleCount,
    3,
    "ruleCount of actual stylesheet is updated correctly"
  );
  assertMediaRules(styleSheetResult.mediaRules, expectedMediaRules);

  targetCommand.destroy();
  await client.close();
}

async function testNestedResourceUpdateFeature() {
  info("Check nested resource update feature of the ResourceCommand");

  const tab = await addTab(STYLE_TEST_URL);

  const {
    outerWidth: originalWindowWidth,
    outerHeight: originalWindowHeight,
  } = tab.ownerGlobal;

  registerCleanupFunction(() => {
    tab.ownerGlobal.resizeTo(originalWindowWidth, originalWindowHeight);
  });

  const { client, resourceWatcher, targetCommand } = await initResourceCommand(
    tab
  );

  info("Setup the watcher");
  const availableResources = [];
  const updates = [];
  await resourceWatcher.watchResources([resourceWatcher.TYPES.STYLESHEET], {
    onAvailable: resources => availableResources.push(...resources),
    onUpdated: newUpdates => updates.push(...newUpdates),
  });
  is(
    availableResources.length,
    EXISTING_RESOURCES.length,
    "Length of existing resources is correct"
  );

  info("Apply new media query");
  // In order to avoid applying the media query (min-height: 400px).
  tab.ownerGlobal.resizeTo(originalWindowWidth, 300);

  // Retrieve the stylesheet of the top-level target
  const resource = availableResources.find(
    innerResource => innerResource.targetFront.isTopLevel
  );
  const styleSheetsFront = await resource.targetFront.getFront("stylesheets");
  await styleSheetsFront.update(
    resource.resourceId,
    "@media (min-height: 400px) { color: red; }",
    false
  );
  await waitUntil(() => updates.length === 3);
  is(resource.mediaRules[0].matches, false, "Media query is not matched yet");

  info("Change window size to fire matches-change event");
  tab.ownerGlobal.resizeTo(originalWindowWidth, 500);
  await waitUntil(() => updates.length === 4);

  // Check the update content.
  const targetUpdate = updates[3];
  assertUpdate(targetUpdate.update, {
    resourceId: resource.resourceId,
    updateType: "matches-change",
  });
  ok(resource === targetUpdate.resource, "Update object has the same resource");

  is(
    JSON.stringify(targetUpdate.update.nestedResourceUpdates[0].path),
    JSON.stringify(["mediaRules", 0, "matches"]),
    "path of nestedResourceUpdates is correct"
  );
  is(
    targetUpdate.update.nestedResourceUpdates[0].value,
    true,
    "value of nestedResourceUpdates is correct"
  );

  // Check the resource.
  const expectedMediaRules = [
    {
      conditionText: "(min-height: 400px)",
      mediaText: "(min-height: 400px)",
      matches: true,
    },
  ];

  assertMediaRules(targetUpdate.resource.mediaRules, expectedMediaRules);

  // Check the actual page.
  const styleSheetResult = await getStyleSheetResult(tab);
  is(
    styleSheetResult.ruleCount,
    1,
    "ruleCount of actual stylesheet is updated correctly"
  );
  assertMediaRules(styleSheetResult.mediaRules, expectedMediaRules);

  tab.ownerGlobal.resizeTo(originalWindowWidth, originalWindowHeight);

  targetCommand.destroy();
  await client.close();
}

function findMatchingExpectedResource(resource) {
  return EXISTING_RESOURCES.find(
    expected =>
      resource.href === expected.href && resource.nodeHref === expected.nodeHref
  );
}

async function getStyleSheetResult(tab) {
  const result = await ContentTask.spawn(tab.linkedBrowser, null, () => {
    const document = content.document;
    const stylesheet = document.styleSheets[0];
    const ruleCount = stylesheet.cssRules.length;

    const mediaRules = [];
    for (const rule of stylesheet.cssRules) {
      if (!rule.media) {
        continue;
      }

      let matches = false;
      try {
        const mql = content.matchMedia(rule.media.mediaText);
        matches = mql.matches;
      } catch (e) {
        // Ignored
      }

      mediaRules.push({
        mediaText: rule.media.mediaText,
        conditionText: rule.conditionText,
        matches,
      });
    }

    return { ruleCount, mediaRules };
  });

  return result;
}

function assertMediaRules(mediaRules, expected) {
  is(mediaRules.length, expected.length, "Length of the mediaRules is correct");

  for (let i = 0; i < mediaRules.length; i++) {
    is(
      mediaRules[i].conditionText,
      expected[i].conditionText,
      "conditionText is correct"
    );
    is(mediaRules[i].mediaText, expected[i].mediaText, "mediaText is correct");
    is(mediaRules[i].matches, expected[i].matches, "matches is correct");

    if (expected[i].line !== undefined) {
      is(mediaRules[i].line, expected[i].line, "line is correct");
    }

    if (expected[i].column !== undefined) {
      is(mediaRules[i].column, expected[i].column, "column is correct");
    }
  }
}

async function assertResource(resource, expected) {
  is(
    resource.resourceType,
    ResourceCommand.TYPES.STYLESHEET,
    "Resource type is correct"
  );
  const styleSheetsFront = await resource.targetFront.getFront("stylesheets");
  const styleText = (
    await styleSheetsFront.getText(resource.resourceId)
  ).str.trim();
  is(styleText, expected.styleText, "Style text is correct");
  is(resource.href, expected.href, "href is correct");
  is(resource.nodeHref, expected.nodeHref, "nodeHref is correct");
  is(resource.isNew, expected.isNew, "isNew is correct");
  is(resource.disabled, expected.disabled, "disabled is correct");
  is(resource.ruleCount, expected.ruleCount, "ruleCount is correct");
  assertMediaRules(resource.mediaRules, expected.mediaRules);
}

function assertUpdate(update, expected) {
  is(
    update.resourceType,
    ResourceCommand.TYPES.STYLESHEET,
    "Resource type is correct"
  );
  is(update.resourceId, expected.resourceId, "resourceId is correct");
  is(update.updateType, expected.updateType, "updateType is correct");
  if (expected.event?.cause) {
    is(update.event?.cause, expected.event.cause, "cause is correct");
  }
}
