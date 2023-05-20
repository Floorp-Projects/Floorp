/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that even when the cache is disabled, the inspector/styleeditor don't fetch again
// stylesheets from the server to display them in devtools, but use the cached version.

const TEST_CSS = URL_ROOT + "browser_toolbox_options_disable_cache.css.sjs";
const TEST_PAGE = `<html>
  <head>
    <meta charset="utf-8"/>
    <link href="${TEST_CSS}" rel="stylesheet" type="text/css"/>
  </head>
  <body></body>
</html>`;

add_task(async function () {
  info("Setup preferences for testing");
  // Disable rcwn to make cache behavior deterministic.
  await pushPref("network.http.rcwn.enabled", false);
  // Disable the cache.
  await pushPref("devtools.cache.disabled", true);

  info("Open inspector");
  const toolbox = await openNewTabAndToolbox(
    `data:text/html;charset=UTF-8,${encodeURIComponent(TEST_PAGE)}`,
    "inspector"
  );
  const inspector = toolbox.getPanel("inspector");

  info(
    "Check that the CSS content loaded in the page " +
      "and the one shown in the inspector are the same"
  );
  const webContent = await getWebContent();
  const inspectorContent = await getInspectorContent(inspector);
  is(
    webContent,
    inspectorContent,
    "The contents of both web and DevTools are same"
  );

  await closeTabAndToolbox();
});

async function getInspectorContent(inspector) {
  const ruleView = inspector.getPanel("ruleview").view;
  const valueEl = await waitFor(() =>
    ruleView.styleDocument.querySelector(".ruleview-propertyvalue")
  );
  return valueEl.textContent;
}

async function getWebContent() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const doc = content.document;
    return doc.ownerGlobal.getComputedStyle(doc.body, "::before").content;
  });
}
