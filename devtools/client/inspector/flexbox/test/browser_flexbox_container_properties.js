/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex container properties are shown when a flex container is selected.

const TEST_URI = `
  <style type='text/css'>
    #container1 {
      display: flex;
    }
  </style>
  <div id="container1">
    <div id="item"></div>
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info(
    "Selecting the flex container #container1 and checking the values of the flex " +
      "container properties for #container1."
  );
  const onFlexContainerPropertiesRendered = waitForDOM(
    doc,
    ".flex-header-container-properties"
  );
  await selectNode("#container1", inspector);
  const [flexContainerProperties] = await onFlexContainerPropertiesRendered;

  ok(flexContainerProperties, "The flex container properties is rendered.");
  is(
    flexContainerProperties.children[0].textContent,
    "row",
    "Got expected flex-direction."
  );
  is(
    flexContainerProperties.children[1].textContent,
    "nowrap",
    "Got expected flex-wrap."
  );

  info(
    "Selecting a flex item and expecting the flex container properties to not be " +
      "shown."
  );
  const onFlexHeaderRendered = waitForDOM(doc, ".flex-header");
  await selectNode("#item", inspector);
  const [flexHeader] = await onFlexHeaderRendered;

  ok(
    !flexHeader.querySelector(".flex-header-container-properties"),
    "The flex container properties is not shown in the header."
  );
});
