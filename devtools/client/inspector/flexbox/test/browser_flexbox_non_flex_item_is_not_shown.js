/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that an element that is the child of a flex container but is not actually a flex
// item is not shown in the sidebar when selected.

const TEST_URI = `
<div style="display:flex">
  <div id="item" style="position:absolute;">test</div>
</div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info("Select the container's supposed flex item.");
  await selectNode("#item", inspector);
  const noFlexContainerOrItemSelected = doc.querySelector(
    ".flex-accordion .devtools-sidepanel-no-result"
  );

  ok(
    noFlexContainerOrItemSelected,
    "The flexbox pane shows a message to select a flex container or item to continue."
  );
});
