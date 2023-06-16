/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that Inspect Element works even if the selector of the inspected
// element changes after opening the context menu.
// For this test, we explicitly move the element when opening the context menu.

const TEST_URL =
  `data:text/html;charset=utf-8,` +
  encodeURIComponent(`

  <div id="parent-1">
    <span>Inspect me</span>
  </div>
  <div id="parent-2"></div>
  <div id="changing-id">My ID will change</div>

  <script type="text/javascript">
    document.querySelector("span").addEventListener("contextmenu", () => {
      // Right after opening the context menu, move the target element in the
      // tree to change its selector.
      window.setTimeout(() => {
        const span = document.querySelector("span");
        document.getElementById("parent-2").appendChild(span);
      }, 0)
    });

    document.querySelector("#changing-id").addEventListener("contextmenu", () => {
      // Right after opening the context menu, update the id of #changing-id
      // to make sure we are not relying on outdated selectors to find the node
      window.setTimeout(() => {
        document.querySelector("#changing-id").id= "new-id";
      }, 0)
    });
  </script>
`);

add_task(async function () {
  await addTab(TEST_URL);
  await testNodeWithChangingPath();
  await testNodeWithChangingId();
});

async function testNodeWithChangingPath() {
  info("Test that inspect element works if the CSS path changes");
  const inspector = await clickOnInspectMenuItem("span");

  const selectedNode = inspector.selection.nodeFront;
  ok(selectedNode, "A node is selected in the inspector");
  is(
    selectedNode.tagName.toLowerCase(),
    "span",
    "The selected node is correct"
  );
  const parentNode = await selectedNode.parentNode();
  is(
    parentNode.id,
    "parent-2",
    "The selected node is under the expected parent node"
  );
}

async function testNodeWithChangingId() {
  info("Test that inspect element works if the id changes");
  const inspector = await clickOnInspectMenuItem("#changing-id");

  const selectedNode = inspector.selection.nodeFront;
  ok(selectedNode, "A node is selected in the inspector");
  is(selectedNode.id.toLowerCase(), "new-id", "The selected node is correct");
}
