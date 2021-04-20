/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test confirms that XUL attributes don't show up as empty
// attributes after being deleted

const TEST_URL = URL_ROOT + "doc_markup_xul.xhtml";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  const panelFront = await getNodeFront("#test", inspector);
  ok(
    panelFront.hasAttribute("id"),
    "panelFront has id attribute in the beginning"
  );

  info("Removing panel's id attribute");
  const onMutation = inspector.once("markupmutation");
  await removeAttributeInBrowser(gBrowser.selectedBrowser, "#test", "id");

  info("Waiting for markupmutation");
  await onMutation;

  is(
    panelFront.hasAttribute("id"),
    false,
    "panelFront doesn't have id attribute anymore"
  );
});
