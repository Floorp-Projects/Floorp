/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around CSS_REGISTERED_PROPERTIES.

const ResourceCommand = require("resource://devtools/shared/commands/resource/resource-command.js");

const IFRAME_URL = `https://example.org/document-builder.sjs?html=${encodeURIComponent(`
  <style>
    @property --css-a {
      syntax: "<color>";
      inherits: true;
      initial-value: gold;
    }
  </style>
  <script>
    CSS.registerProperty({
      name: "--js-a",
      syntax: "<length>",
      inherits: true,
      initialValue: "20px"
    });
  </script>
  <h1>iframe</h1>
`)}`;

const TEST_URL = `https://example.org/document-builder.sjs?html=
  <head>
    <style>
      @property --css-a {
        syntax: "*";
        inherits: false;
      }

      @property --css-b {
        syntax: "<color>";
        inherits: true;
        initial-value: tomato;
      }
    </style>
    <script>
      CSS.registerProperty({
        name: "--js-a",
        syntax: "*",
        inherits: false,
      });
      CSS.registerProperty({
        name: "--js-b",
        syntax: "<length>",
        inherits: true,
        initialValue: "10px"
      });
    </script>
  </head>
  <h1>CSS_REGISTERED_PROPERTIES</h1>
  <iframe src="${encodeURIComponent(IFRAME_URL)}"></iframe>`;

add_task(async function () {
  await pushPref("layout.css.properties-and-values.enabled", true);
  const tab = await addTab(TEST_URL);

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  // Wait for targets
  await targetCommand.startListening();
  const targets = [];
  const onAvailable = ({ targetFront }) => targets.push(targetFront);
  await targetCommand.watchTargets({
    types: [targetCommand.TYPES.FRAME],
    onAvailable,
  });
  await waitFor(() => targets.length === 2);
  const [topLevelTarget, iframeTarget] = targets.sort((a, _) =>
    a.isTopLevel ? -1 : 1
  );

  // Watching for new stylesheets shouldn't be
  const stylesheets = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.STYLESHEET], {
    onAvailable: resources => stylesheets.push(...resources),
    ignoreExistingResources: true,
  });

  info("Check that we get existing registered properties");
  const availableResources = [];
  const updatedResources = [];
  const destroyedResources = [];
  await resourceCommand.watchResources(
    [resourceCommand.TYPES.CSS_REGISTERED_PROPERTIES],
    {
      onAvailable: resources => availableResources.push(...resources),
      onUpdated: resources => updatedResources.push(...resources),
      onDestroyed: resources => destroyedResources.push(...resources),
    }
  );

  is(
    availableResources.length,
    6,
    "The 6 existing registered properties where retrieved"
  );

  // Sort resources so we get them alphabetically ordered by their name, with the ones for
  // the top level target displayed first.
  availableResources.sort((a, b) => {
    if (a.targetFront !== b.targetFront) {
      return a.targetFront.isTopLevel ? -1 : 1;
    }
    return a.name < b.name ? -1 : 1;
  });

  assertResource(availableResources[0], {
    name: "--css-a",
    syntax: "*",
    inherits: false,
    initialValue: null,
    fromJS: false,
    targetFront: topLevelTarget,
  });
  assertResource(availableResources[1], {
    name: "--css-b",
    syntax: "<color>",
    inherits: true,
    initialValue: "tomato",
    fromJS: false,
    targetFront: topLevelTarget,
  });
  assertResource(availableResources[2], {
    name: "--js-a",
    syntax: "*",
    inherits: false,
    initialValue: null,
    fromJS: true,
    targetFront: topLevelTarget,
  });
  assertResource(availableResources[3], {
    name: "--js-b",
    syntax: "<length>",
    inherits: true,
    initialValue: "10px",
    fromJS: true,
    targetFront: topLevelTarget,
  });
  assertResource(availableResources[4], {
    name: "--css-a",
    syntax: "<color>",
    inherits: true,
    initialValue: "gold",
    fromJS: false,
    targetFront: iframeTarget,
  });
  assertResource(availableResources[5], {
    name: "--js-a",
    syntax: "<length>",
    inherits: true,
    initialValue: "20px",
    fromJS: true,
    targetFront: iframeTarget,
  });

  info("Check that we didn't get notified about existing stylesheets");
  // wait a bit so we'd have the time to be notified about stylesheet resources
  await wait(500);
  is(
    stylesheets.length,
    0,
    "Watching for registered properties does not notify about existing stylesheets resources"
  );

  info("Check that we get properties from new stylesheets");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const s = content.document.createElement("style");
    s.textContent = `
        @property --css-c {
          syntax: "<custom-ident>";
          inherits: true;
          initial-value: custom;
        }

        @property --css-d {
          syntax: "big | bigger";
          inherits: true;
          initial-value: big;
        }
    `;
    content.document.head.append(s);
  });

  info("Wait for registered properties to be available");
  await waitFor(() => availableResources.length === 8);
  ok(true, "Got notified about 2 new registered properties");
  assertResource(availableResources[6], {
    name: "--css-c",
    syntax: "<custom-ident>",
    inherits: true,
    initialValue: "custom",
    fromJS: false,
    targetFront: topLevelTarget,
  });
  assertResource(availableResources[7], {
    name: "--css-d",
    syntax: "big | bigger",
    inherits: true,
    initialValue: "big",
    fromJS: false,
    targetFront: topLevelTarget,
  });

  info("Wait to be notified about the new stylesheet");
  await waitFor(() => stylesheets.length === 1);
  ok(true, "we do get notified about stylesheets");

  info(
    "Check that we get notified about properties registered via CSS.registerProperty"
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.CSS.registerProperty({
      name: "--js-c",
      syntax: "*",
      inherits: false,
      initialValue: 42,
    });
    content.CSS.registerProperty({
      name: "--js-d",
      syntax: "<color>#",
      inherits: true,
      initialValue: "blue,cyan",
    });
  });

  await waitFor(() => availableResources.length === 10);
  ok(true, "Got notified about 2 new registered properties");
  assertResource(availableResources[8], {
    name: "--js-c",
    syntax: "*",
    inherits: false,
    initialValue: "42",
    fromJS: true,
    targetFront: topLevelTarget,
  });
  assertResource(availableResources[9], {
    name: "--js-d",
    syntax: "<color>#",
    inherits: true,
    initialValue: "blue,cyan",
    fromJS: true,
    targetFront: topLevelTarget,
  });

  info(
    "Check that we get notified about properties registered via CSS.registerProperty in iframe"
  );
  const iframeBrowsingContext = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => content.document.querySelector("iframe").browsingContext
  );

  await SpecialPowers.spawn(iframeBrowsingContext, [], () => {
    content.CSS.registerProperty({
      name: "--js-iframe",
      syntax: "<color>#",
      inherits: true,
      initialValue: "red,salmon",
    });
  });

  await waitFor(() => availableResources.length === 11);
  ok(true, "Got notified about 2 new registered properties");
  assertResource(availableResources[10], {
    name: "--js-iframe",
    syntax: "<color>#",
    inherits: true,
    initialValue: "red,salmon",
    fromJS: true,
    targetFront: iframeTarget,
  });

  info(
    "Check that we get notified about destroyed properties when removing stylesheet"
  );
  // sanity check
  is(destroyedResources.length, 0, "No destroyed resources yet");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.querySelector("style").remove();
  });
  await waitFor(() => destroyedResources.length == 2);
  ok(true, "We got notified about destroyed resources");
  destroyedResources.sort((a, b) => a < b);
  is(
    destroyedResources[0].resourceType,
    ResourceCommand.TYPES.CSS_REGISTERED_PROPERTIES,
    "resource type is correct"
  );
  is(
    destroyedResources[0].resourceId,
    `${topLevelTarget.actorID}:css-registered-property:--css-a`,
    "expected css property was destroyed"
  );
  is(
    destroyedResources[1].resourceType,
    ResourceCommand.TYPES.CSS_REGISTERED_PROPERTIES,
    "resource type is correct"
  );
  is(
    destroyedResources[1].resourceId,
    `${topLevelTarget.actorID}:css-registered-property:--css-b`,
    "expected css property was destroyed"
  );

  info(
    "Check that we get notified about updated properties when modifying stylesheet"
  );
  is(updatedResources.length, 0, "No updated resources yet");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.querySelector("style").textContent = `
     /* not updated */
      @property --css-c {
        syntax: "<custom-ident>";
        inherits: true;
        initial-value: custom;
      }

      @property --css-d {
        syntax: "big | bigger";
        inherits: true;
        /* only change initial value (was big) */
        initial-value: bigger;
      }

      /* add a new property */
      @property --css-e {
        syntax: "<color>";
        inherits: false;
        initial-value: green;
      }
    `;
  });
  await waitFor(() => updatedResources.length === 1);
  ok(true, "One property was updated");
  assertResource(updatedResources[0].resource, {
    name: "--css-d",
    syntax: "big | bigger",
    inherits: true,
    initialValue: "bigger",
    fromJS: false,
    targetFront: topLevelTarget,
  });

  await waitFor(() => availableResources.length === 12);
  ok(true, "We got notified about the new property");
  assertResource(availableResources.at(-1), {
    name: "--css-e",
    syntax: "<color>",
    inherits: false,
    initialValue: "green",
    fromJS: false,
    targetFront: topLevelTarget,
  });

  await client.close();
});

async function assertResource(resource, expected) {
  is(
    resource.resourceType,
    ResourceCommand.TYPES.CSS_REGISTERED_PROPERTIES,
    "Resource type is correct"
  );
  is(resource.name, expected.name, "name is correct");
  is(resource.syntax, expected.syntax, "syntax is correct");
  is(resource.inherits, expected.inherits, "inherits is correct");
  is(resource.initialValue, expected.initialValue, "initialValue is correct");
  is(resource.fromJS, expected.fromJS, "fromJS is correct");
  is(
    resource.targetFront,
    expected.targetFront,
    "resource is associated with expected target"
  );
}
