/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the markup view displaying the content of a <template> tag.

add_task(async function() {
  const TEST_URL = `data:text/html;charset=utf-8,` + encodeURIComponent(`
    <div id="root">
      <template>
        <p>template content</p>
      </template>

      <div id="template-container" style="border: 1px solid black"></div>
    </div>
    <script>
      "use strict";

      const template = document.querySelector("template");
      const clone = document.importNode(template.content, true);
      document.querySelector("#template-container").appendChild(clone);
    </script>`);

  const EXPECTED_TREE = `
    root
      template
        #document-fragment
          p
      template-container
        p`;

  const {inspector} = await openInspectorForURL(TEST_URL);
  const {markup} = inspector;

  await assertMarkupViewAsTree(EXPECTED_TREE, "#root", inspector);

  info("Select the p element under the template .");
  const templateFront = await getNodeFront("template", inspector);
  const templateContainer = markup.getContainer(templateFront);
  const documentFragmentContainer = templateContainer.getChildContainers()[0];
  const pContainer = documentFragmentContainer.getChildContainers()[0];

  await selectNode(pContainer.node, inspector, "no-reason", false);

  const ruleView = inspector.getPanel("ruleview").view;
  is(ruleView.element.querySelectorAll("#ruleview-no-results").length, 1,
    "No rules are displayed for this p element");
});
