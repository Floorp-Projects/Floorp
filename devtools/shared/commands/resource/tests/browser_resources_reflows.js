/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API for reflows

const {
  TYPES,
} = require("resource://devtools/shared/commands/resource/resource-command.js");

add_task(async function () {
  const tab = await addTab(
    "https://example.com/document-builder.sjs?html=<h1>Test reflow resources</h1>"
  );

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  const resources = [];
  const onAvailable = _resources => {
    resources.push(..._resources);
  };
  await resourceCommand.watchResources([TYPES.REFLOW], {
    onAvailable,
  });

  is(resources.length, 0, "No reflow resource were sent initially");

  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    const el = content.document.createElement("div");
    el.textContent = "1";
    content.document.body.appendChild(el);
  });

  await waitFor(() => resources.length === 1);
  checkReflowResource(resources[0]);

  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    const el = content.document.querySelector("div");
    el.style.display = "inline-grid";
  });

  await waitFor(() => resources.length === 2);
  ok(
    true,
    "A reflow resource is sent when the display property of an element is modified"
  );
  checkReflowResource(resources.at(-1));

  info("Check that adding an iframe does emit a reflow");
  const iframeBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      const el = content.document.createElement("iframe");
      const onIframeLoaded = new Promise(resolve =>
        el.addEventListener("load", resolve, { once: true })
      );
      content.document.body.appendChild(el);
      el.src =
        "https://example.org/document-builder.sjs?html=<h2>remote iframe</h2>";
      await onIframeLoaded;
      return el.browsingContext;
    }
  );

  await waitFor(() => resources.length === 3);
  ok(true, "A reflow resource was received when adding a remote iframe");
  checkReflowResource(resources.at(-1));

  info("Check that we receive reflow resources for the remote iframe");
  await SpecialPowers.spawn(iframeBC, [], () => {
    const el = content.document.createElement("section");
    el.textContent = "remote org iframe";
    el.style.display = "grid";
    content.document.body.appendChild(el);
  });

  await waitFor(() => resources.length === 4);
  if (isFissionEnabled()) {
    ok(
      resources.at(-1).targetFront.url.includes("example.org"),
      "The reflow resource is linked to the remote target"
    );
  }
  checkReflowResource(resources.at(-1));

  targetCommand.destroy();
  await client.close();
});

function checkReflowResource(resource) {
  is(
    resource.resourceType,
    TYPES.REFLOW,
    "The resource has the expected resourceType"
  );

  ok(Array.isArray(resource.reflows), "the `reflows` property is an array");
  for (const reflow of resource.reflows) {
    is(
      Number.isFinite(reflow.start),
      true,
      "reflow start property is a number"
    );
    is(Number.isFinite(reflow.end), true, "reflow end property is a number");
    Assert.greaterOrEqual(
      reflow.end,
      reflow.start,
      "end is greater than start"
    );
  }
}
