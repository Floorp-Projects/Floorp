/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test the locations of the filter buttons in the Webconsole's Filter Bar.

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
    "test/mochitest/test-console.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const win = hud.browserWindow;
  const initialWindowWidth = win.outerWidth;

  info("Check filter buttons are inline with filter input when window is large.");
  resizeWindow(1500, win);
  let primaryFilterBar = await getPrimaryFilterBar(hud);
  let filterButtons = await getSecondaryFilterBar(hud);
  let filterInput = await getFilterInput(hud);
  checkFilterInputWidth(filterInput);
  is(filterButtons.parentNode, primaryFilterBar,
    "The filter buttons div should be a child of the primary filter bar.");
  is(filterButtons.previousSibling.previousSibling, filterInput,
    "The filter buttons div should appear after a divider after the filter input.");

  info("Check filter buttons overflow when window is small.");
  resizeWindow(400, win);
  primaryFilterBar = await getPrimaryFilterBar(hud);
  filterButtons = await getSecondaryFilterBar(hud);
  filterInput = await getFilterInput(hud);
  checkFilterInputWidth(filterInput);
  is(filterButtons.parentNode, primaryFilterBar.parentNode,
    "The filter buttons div should share the same parent as the primary filter bar.");
  for (const sibling of primaryFilterBar.childNodes) {
    isnot(filterButtons, sibling,
      "The filter buttons should not be in the same div as the filter input");
  }

  info("Restore the original window size");
  await resizeWindow(initialWindowWidth, win);

  await closeTabAndToolbox();
});

async function resizeWindow(width, win) {
  const onResize = once(win, "resize");
  win.resizeTo(width, win.outerHeight);
  info("Wait for window resize event");
  await onResize;
}

async function getElementBySelector(hud, query) {
  const outputNode = hud.ui.outputNode;

  const element = await waitFor(() => {
    return outputNode.querySelector(query);
  });

  return element;
}

async function getPrimaryFilterBar(hud) {
  info("Wait for console filterbar to appear.");
  return getElementBySelector(hud, ".webconsole-filterbar-primary");
}

async function getSecondaryFilterBar(hud) {
  info("Wait for filter buttons to appear.");
  return getElementBySelector(hud, ".webconsole-filterbar-secondary");
}

async function getFilterInput(hud) {
  info("Wait for fitler input to appear.");
  return getElementBySelector(hud, ".devtools-searchbox");
}

async function checkFilterInputWidth(input) {
  const minInputWidth = 250;
  const inputWidth = parseInt(getComputedStyle(input).width, 10);

  ok(inputWidth >= minInputWidth, "Filter input should be greater than 250.");
}
