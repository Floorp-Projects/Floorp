/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const dataUrl = new Array(1000 * 1000).join("a");
const TEST_URL = `data:text/html,<style>body{background-image:url(data:image/png;base64,${dataUrl}) no-repeat}`;

// Check that long URLs are rendered correctly in the rule view.
add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const view = selectRuleView(inspector);

  await selectNode("body", inspector);

  const propertyValue = view.styleDocument.querySelector(
    ".ruleview-propertyvalue"
  );
  ok(
    propertyValue.textContent.startsWith("url(data:image/png"),
    "Property value for the background image was correctly rendered"
  );
});
