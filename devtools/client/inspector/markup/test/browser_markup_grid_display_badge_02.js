/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests toggling multiple grid highlighters in the markup view with the grid display
// badges.

const TEST_URI = `
  <style type='text/css'>
    .grid {
      display: grid;
    }
  </style>
  <div id="grid1" class="grid">
    <div class="cell1">cell1</div>
    <div class="cell2">cell2</div>
  </div>
  <div id="grid2" class="grid">
    <div class="cell1">cell1</div>
    <div class="cell2">cell2</div>
  </div>
  <div id="grid3" class="grid">
    <div class="cell1">cell1</div>
    <div class="cell2">cell2</div>
  </div>
`;

add_task(async function() {
  await pushPref("devtools.gridinspector.maxHighlighters", 2);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector } = await openLayoutView();
  const { highlighters } = inspector;

  const grid1 = await getContainerForSelector("#grid1", inspector);
  const grid2 = await getContainerForSelector("#grid2", inspector);
  const grid3 = await getContainerForSelector("#grid3", inspector);
  const gridDisplayBadge1 = grid1.elt.querySelector(
    ".inspector-badge.interactive[data-display]"
  );
  const gridDisplayBadge2 = grid2.elt.querySelector(
    ".inspector-badge.interactive[data-display]"
  );
  const gridDisplayBadge3 = grid3.elt.querySelector(
    ".inspector-badge.interactive[data-display]"
  );

  info(
    "Check the initial state of the grid display badges and grid highlighters"
  );
  ok(
    !gridDisplayBadge1.classList.contains("active"),
    "#grid1 display badge is not active."
  );
  ok(
    !gridDisplayBadge2.classList.contains("active"),
    "#grid2 display badge is not active."
  );
  ok(
    !gridDisplayBadge3.classList.contains("active"),
    "#grid3 display badge is not active."
  );
  ok(
    gridDisplayBadge1.classList.contains("interactive"),
    "#grid1 display badge is interactive"
  );
  ok(
    gridDisplayBadge2.classList.contains("interactive"),
    "#grid2 display badge is interactive"
  );
  ok(
    gridDisplayBadge3.classList.contains("interactive"),
    "#grid3 display badge is interactive"
  );
  ok(
    !highlighters.gridHighlighters.size,
    "No CSS grid highlighter exists in the highlighters overlay."
  );

  info("Toggling ON the CSS grid highlighter from the #grid1 display badge.");
  let onHighlighterShown = highlighters.once("grid-highlighter-shown");
  gridDisplayBadge1.click();
  await onHighlighterShown;

  ok(
    gridDisplayBadge1.classList.contains("active"),
    "#grid1 display badge is active."
  );
  ok(
    !gridDisplayBadge2.classList.contains("active"),
    "#grid2 display badge is not active."
  );
  ok(
    !gridDisplayBadge3.classList.contains("active"),
    "#grid3 display badge is not active."
  );
  ok(
    gridDisplayBadge1.classList.contains("interactive"),
    "#grid1 display badge is interactive"
  );
  ok(
    gridDisplayBadge2.classList.contains("interactive"),
    "#grid2 display badge is interactive"
  );
  ok(
    gridDisplayBadge3.classList.contains("interactive"),
    "#grid3 display badge is interactive"
  );
  is(
    highlighters.gridHighlighters.size,
    1,
    "Got expected number of grid highlighters shown."
  );

  info("Toggling ON the CSS grid highlighter from the #grid2 display badge.");
  onHighlighterShown = highlighters.once("grid-highlighter-shown");
  gridDisplayBadge2.click();
  await onHighlighterShown;

  ok(
    gridDisplayBadge1.classList.contains("active"),
    "#grid1 display badge is active."
  );
  ok(
    gridDisplayBadge2.classList.contains("active"),
    "#grid2 display badge is active."
  );
  ok(
    !gridDisplayBadge3.classList.contains("active"),
    "#grid3 display badge is not active."
  );
  ok(
    gridDisplayBadge1.classList.contains("interactive"),
    "#grid1 display badge is interactive"
  );
  ok(
    gridDisplayBadge2.classList.contains("interactive"),
    "#grid2 display badge is interactive"
  );
  ok(
    !gridDisplayBadge3.classList.contains("interactive"),
    "#grid3 display badge is not interactive"
  );
  is(
    highlighters.gridHighlighters.size,
    2,
    "Got expected number of grid highlighters shown."
  );

  info(
    "Attempt to toggle ON the CSS grid highlighter from the #grid3 display badge."
  );
  gridDisplayBadge3.click();

  ok(
    gridDisplayBadge1.classList.contains("active"),
    "#grid1 display badge is active."
  );
  ok(
    gridDisplayBadge2.classList.contains("active"),
    "#grid2 display badge is active."
  );
  ok(
    !gridDisplayBadge3.classList.contains("active"),
    "#grid3 display badge is not active."
  );
  ok(
    gridDisplayBadge1.classList.contains("interactive"),
    "#grid1 display badge is interactive"
  );
  ok(
    gridDisplayBadge2.classList.contains("interactive"),
    "#grid2 display badge is interactive"
  );
  ok(
    !gridDisplayBadge3.classList.contains("interactive"),
    "#grid3 display badge is not interactive"
  );
  is(
    highlighters.gridHighlighters.size,
    2,
    "Got expected number of grid highlighters shown."
  );

  info("Toggling OFF the CSS grid highlighter from the #grid2 display badge.");
  let onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  gridDisplayBadge2.click();
  await onHighlighterHidden;

  ok(
    gridDisplayBadge1.classList.contains("active"),
    "#grid1 display badge is active."
  );
  ok(
    !gridDisplayBadge2.classList.contains("active"),
    "#grid2 display badge is not active."
  );
  ok(
    !gridDisplayBadge3.classList.contains("active"),
    "#grid3 display badge is not active."
  );
  ok(
    gridDisplayBadge1.classList.contains("interactive"),
    "#grid1 display badge is interactive"
  );
  ok(
    gridDisplayBadge2.classList.contains("interactive"),
    "#grid2 display badge is interactive"
  );
  ok(
    gridDisplayBadge3.classList.contains("interactive"),
    "#grid3 display badge is interactive"
  );
  is(
    highlighters.gridHighlighters.size,
    1,
    "Got expected number of grid highlighters shown."
  );

  info("Toggling OFF the CSS grid highlighter from the #grid1 display badge.");
  onHighlighterHidden = highlighters.once("grid-highlighter-hidden");
  gridDisplayBadge1.click();
  await onHighlighterHidden;

  ok(
    !gridDisplayBadge1.classList.contains("active"),
    "#grid1 display badge is not active."
  );
  ok(
    !gridDisplayBadge2.classList.contains("active"),
    "#grid2 display badge is not active."
  );
  ok(
    !gridDisplayBadge3.classList.contains("active"),
    "#grid3 display badge is not active."
  );
  ok(
    gridDisplayBadge1.classList.contains("interactive"),
    "#grid1 display badge is interactive"
  );
  ok(
    gridDisplayBadge2.classList.contains("interactive"),
    "#grid2 display badge is interactive"
  );
  ok(
    gridDisplayBadge3.classList.contains("interactive"),
    "#grid3 display badge is interactive"
  );
  ok(
    !highlighters.gridHighlighters.size,
    "No CSS grid highlighter exists in the highlighters overlay."
  );
});
