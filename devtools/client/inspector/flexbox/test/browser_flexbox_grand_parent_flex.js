/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a flex container is not shown as a flex item of its grandparent flex
// container.

const TEST_URI = `
<style>
.flex {
  display: flex;
}
</style>
<div class="flex">
  <div>
    <div id="grandchild" class="flex">
      This is a flex item of a flex container.
      Its parent isn't a flex container, but its grandparent is.
    </div>
  </div>
</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info("Select the flex container's grandchild.");
  const onFlexContainerHeaderRendered = waitForDOM(
    doc,
    ".flex-header-container-label"
  );
  await selectNode("#grandchild", inspector);
  await onFlexContainerHeaderRendered;

  info("Check that only the Flex Container accordion item is showing.");
  const flexPanes = doc.querySelectorAll(".flex-accordion");
  is(
    flexPanes.length,
    1,
    "There should only be one flex accordion item showing."
  );

  info("Check that the container header shows Flex Container.");
  const flexAccordionHeader = flexPanes[0].querySelector("._header .truncate");
  is(
    flexAccordionHeader.textContent,
    "Flex Container",
    "The flexbox pane shows a flex container accordion item."
  );
});
