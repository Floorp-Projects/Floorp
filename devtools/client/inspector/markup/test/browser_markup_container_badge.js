/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the container badge is displayed on element with expected container-type values.

const TEST_URI = `
  <style type="text/css">
    #container-inline-size {
      container-type: inline-size;
    }

    #container-size {
      container-type: size;
    }

    #container-normal {
      container-type: normal;
    }
  </style>
  <div id="container-inline-size">container-inline-size</div>
  <div id="container-size">container-size</div>
  <div id="container-normal">container-normal</div>
`;

add_task(async function () {
  await pushPref("layout.css.container-queries.enabled", true);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector } = await openLayoutView();

  info(
    "Check that the container badge is shown for element with container-type: inline-size"
  );
  let container = await getContainerForSelector(
    "#container-inline-size",
    inspector
  );
  const containerInlineSizeBadge = container.elt.querySelector(
    ".inspector-badge[data-container]"
  );
  ok(
    !!containerInlineSizeBadge,
    "container badge is displayed for inline-size container"
  );
  is(
    containerInlineSizeBadge.textContent,
    "container",
    "badge has expected text"
  );
  is(
    containerInlineSizeBadge.title,
    "container-type: inline-size",
    "badge has expected title for inline-size container"
  );

  info("Change the element containerType value to see if the badge hides");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.document.querySelector(
      "#container-inline-size"
    ).style.containerType = "normal";
  });
  await waitFor(
    () =>
      container.elt.querySelector(".inspector-badge[data-container]") == null
  );
  ok(true, "The badge hides when changing the containerType value");

  info(
    "Check that the container badge is shown for element with container-type: size"
  );
  container = await getContainerForSelector("#container-size", inspector);
  const containerSizeBadge = container.elt.querySelector(
    ".inspector-badge[data-container]"
  );
  ok(!!containerSizeBadge, "container badge is displayed for size container");
  is(containerSizeBadge.textContent, "container", "badge has expected text");
  is(
    containerSizeBadge.title,
    "container-type: size",
    "badge has expected title for size container"
  );

  info(
    "Check that the container badge is not shown for element with container-type: normal"
  );
  container = await getContainerForSelector("#container-normal", inspector);
  const noContainerBadge = container.elt.querySelector(
    ".inspector-badge[data-container]"
  );
  ok(
    !noContainerBadge,
    "container badge is not displayed for element with container-type: normal"
  );
});
