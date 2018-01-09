/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
`data:text/html;charset=utf-8,
  <p>Web Console test for scroll when filtering.</p>
  <script>
  for (let i = 0; i < 100; i++) {
    console.log("init-" + i);
  }
  </script>
`;
add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  let {ui} = hud;
  const outputContainer = ui.outputNode.querySelector(".webconsole-output");

  info("Console should be scrolled to bottom on initial load from page logs");
  await waitFor(() => findMessage(hud, "init-99"));
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  ok(isScrolledToBottom(outputContainer), "The console is scrolled to the bottom");

  info("Filter out some messages and check that the scroll position is not impacted");
  const filterInput = hud.ui.outputNode.querySelector(".text-filter");

  filterInput.value = "init-";
  filterInput.focus();
  let onMessagesFiltered = waitFor(() => !findMessage(hud, "init-1"), null, 200);
  EventUtils.synthesizeKey("9", {});
  await onMessagesFiltered;
  ok(isScrolledToBottom(outputContainer),
    "The console is still scrolled to the bottom after filtering");

  info("Clear the text filter and check that the scroll position is not impacted");
  let onMessagesUnFiltered = waitFor(() => findMessage(hud, "init-1"), null, 200);
  filterInput.select();
  EventUtils.synthesizeKey("VK_DELETE", {});
  await onMessagesUnFiltered;
  ok(isScrolledToBottom(outputContainer),
    "The console is still scrolled to the bottom after clearing the filter");

  info("Scroll up");
  outputContainer.scrollTop = 0;

  filterInput.value = "init-";
  filterInput.focus();
  onMessagesFiltered = waitFor(() => !findMessage(hud, "init-1"), null, 200);
  EventUtils.synthesizeKey("9", {});
  await onMessagesFiltered;
  is(outputContainer.scrollTop, 0,
    "The console is still scrolled to the top after filtering");

  info("Clear the text filter and check that the scroll position is not impacted");
  onMessagesUnFiltered = waitFor(() => findMessage(hud, "init-1"), null, 200);
  filterInput.select();
  EventUtils.synthesizeKey("VK_DELETE", {});
  await onMessagesUnFiltered;
  is(outputContainer.scrollTop, 0,
    "The console is still scrolled to the top after clearing the filter");

});

function hasVerticalOverflow(container) {
  return container.scrollHeight > container.clientHeight;
}

function isScrolledToBottom(container) {
  if (!container.lastChild) {
    return true;
  }
  let lastNodeHeight = container.lastChild.clientHeight;
  return container.scrollTop + container.clientHeight >=
         container.scrollHeight - lastNodeHeight / 2;
}
