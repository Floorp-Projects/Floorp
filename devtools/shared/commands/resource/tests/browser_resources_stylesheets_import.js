/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API for imported STYLESHEET + iframe.

const styleSheetText = `
@import "${URL_ROOT_ORG_SSL}/style_document.css";
body { background-color: tomato; }`;

const IFRAME_URL = `https://example.org/document-builder.sjs?html=${encodeURIComponent(`
  <style>${styleSheetText}</style>
  <h1>iframe</h1>
`)}`;

const TEST_URL = `https://example.org/document-builder.sjs?html=
  <h1>import stylesheet test</h1>
  <iframe src="${encodeURIComponent(IFRAME_URL)}"></iframe>`;

add_task(async function () {
  info("Check resource available feature of the ResourceCommand");

  const tab = await addTab(TEST_URL);

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  info("Check whether ResourceCommand gets existing stylesheet");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.STYLESHEET], {
    onAvailable: resources => availableResources.push(...resources),
  });

  await waitFor(() => availableResources.length === 2);
  ok(true, "We're getting the expected stylesheets");

  const styleNodeStyleSheet = availableResources.find(
    resource => resource.nodeHref
  );
  const importedStyleSheet = availableResources.find(
    resource => resource !== styleNodeStyleSheet
  );

  is(
    await getStyleSheetText(styleNodeStyleSheet),
    styleSheetText.trim(),
    "Got expected text for the <style> stylesheet"
  );

  is(
    await getStyleSheetText(importedStyleSheet),
    `body { margin: 1px; }`,
    "Got expected text for the imported stylesheet"
  );

  targetCommand.destroy();
  await client.close();
});

async function getStyleSheetText(resource) {
  const styleSheetsFront = await resource.targetFront.getFront("stylesheets");
  const styleText = await styleSheetsFront.getText(resource.resourceId);
  return styleText.str.trim();
}
