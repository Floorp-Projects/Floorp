/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Tests that the overflow badge is shown to overflow causing elements and is updated
// dynamically.

const TEST_URI = `
  <style type="text/css">
    .parent {
        width: 200px;
        height: 200px;
        overflow: scroll;
    }
    .fixed {
        width: 50px;
        height: 50px;
    }
    .shift {
        margin-left: 300px;
    }
  </style>
  <div id="top" class="parent">
    <div id="child1" class="fixed shift">
        <div id="child2" class="fixed"></div>
    </div>
    <div id="child3" class="shift">
        <div id="child4" class="fixed"></div>
    </div>
  </div>
`;

add_task(async function () {
  const { inspector } = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  await expandChildContainers(inspector);

  const child1 = await getContainerForSelector("#child1", inspector);
  const child2 = await getContainerForSelector("#child2", inspector);
  const child3 = await getContainerForSelector("#child3", inspector);
  const child4 = await getContainerForSelector("#child4", inspector);

  info(
    "Checking if overflow badges appear correctly right after the markup-view is loaded"
  );

  checkBadges([child1, child4], [child2, child3]);

  info("Changing the elements to check whether the badge updates correctly");

  await toggleClass(inspector);

  checkBadges([child2, child3], [child1, child4]);

  info(
    "Once again, changing the elements to check whether the badge updates correctly"
  );

  await toggleClass(inspector);

  checkBadges([child1, child4], [child2, child3]);
});

async function expandChildContainers(inspector) {
  const container = await getContainerForSelector("#top", inspector);

  await expandContainer(inspector, container);
  for (const child of container.getChildContainers()) {
    await expandContainer(inspector, child);
  }
}

async function toggleClass(inspector) {
  const onStateChanged = inspector.walker.once("overflow-change");

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.document.querySelector("#child1").classList.toggle("fixed");
    content.document.querySelector("#child3").classList.toggle("fixed");
  });

  await onStateChanged;
}

function checkBadges(elemsWithBadges, elemsWithNoBadges) {
  const hasBadge = elem =>
    elem.elt.querySelector(
      `#${elem.elt.children[0].id} > span.editor > .inspector-badge.overflow-badge`
    );

  for (const elem of elemsWithBadges) {
    ok(hasBadge(elem), `Overflow badge exists in ${elem.node.id}`);
  }

  for (const elem of elemsWithNoBadges) {
    ok(!hasBadge(elem), `Overflow badge does not exist in ${elem.node.id}`);
  }
}
