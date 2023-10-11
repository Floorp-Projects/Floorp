/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that the event listeners popup can be used from the keyboard.

const TEST_URL = URL_ROOT_SSL + "doc_markup_events_toggle.html";

loadHelperScript("helper_events_test_runner.js");

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);
  await inspector.markup.expandAll();
  await selectNode("#target", inspector);

  info("Check that the event tooltip has the expected content");
  const container = await getContainerForSelector("#target", inspector);
  const eventTooltipBadge = container.elt.querySelector(
    ".inspector-badge.interactive[data-event]"
  );
  ok(eventTooltipBadge, "The event tooltip badge is displayed");
  is(
    eventTooltipBadge.getAttribute("aria-pressed"),
    "false",
    "The event tooltip badge is not pressed"
  );

  const tooltip = inspector.markup.eventDetailsTooltip;
  const onTooltipShown = tooltip.once("shown");
  eventTooltipBadge.focus();
  EventUtils.synthesizeKey("VK_RETURN", {}, eventTooltipBadge.ownerGlobal);
  await onTooltipShown;
  ok(true, "The tooltip is shown");
  is(
    eventTooltipBadge.getAttribute("aria-pressed"),
    "true",
    "The event tooltip badge is pressed"
  );

  const tooltipDoc = tooltip.panel.ownerDocument;
  const tooltipWin = tooltip.panel.ownerGlobal;
  const tooltipContainerEl = tooltip.panel.querySelector(
    ".devtools-tooltip-events-container"
  );
  const tooltipItems = Array.from(tooltipContainerEl.querySelectorAll("li"));
  const twistyButton = tooltipItems[0].querySelector("button.theme-twisty");
  is(
    tooltipDoc.activeElement,
    twistyButton,
    "Focus is set on first twisty button"
  );
  ok(true, "Focus was moved to the twisty button");
  is(
    twistyButton.getAttribute("title"),
    "“click” event listener code",
    "Twisty button has expected title"
  );
  is(
    twistyButton.getAttribute("aria-expanded"),
    "false",
    "Twisty button is not expanded"
  );
  is(
    twistyButton.getAttribute("aria-owns"),
    "cm-0",
    "Twisty button has expected aria-owns attribute"
  );
  const cmIframeContainer = tooltipDoc.getElementById(
    twistyButton.getAttribute("aria-owns")
  );
  ok(!!cmIframeContainer, "Twisty button aria-owns points to expected element");

  info("Press Enter key to show event listener code");
  EventUtils.synthesizeKey("VK_RETURN", {}, tooltipWin);
  await waitFor(() => twistyButton.getAttribute("aria-expanded") === "true");
  ok(true, "Twisty button is now expanded");
  ok(
    cmIframeContainer.hasAttribute("open"),
    "iframe container has open attribute"
  );

  is(
    cmIframeContainer.querySelector("iframe").getAttribute("title"),
    "“click” event listener code"
  );

  info("Press Enter key again to hide event listener code");
  EventUtils.synthesizeKey("VK_RETURN", {}, tooltipWin);
  await waitFor(() => twistyButton.getAttribute("aria-expanded") === "false");
  ok(true, "Twisty button is no longer expanded");
  ok(
    !cmIframeContainer.hasAttribute("open"),
    "iframe container no longer has open attribute"
  );

  info("Press Tab key to focus first Open in debugger button");
  EventUtils.synthesizeKey("VK_TAB", {}, tooltipWin);
  const openInDebuggerButton = tooltipItems[0].querySelector(
    "button.event-tooltip-debugger-icon"
  );
  is(
    tooltipDoc.activeElement,
    openInDebuggerButton,
    "Focus was moved to the Open in Debugger button"
  );
  is(
    openInDebuggerButton.getAttribute("title"),
    "Open “click” in Debugger",
    "Open in Debugger button has expected title"
  );

  info("Press Tab key to focus first checkbox");
  EventUtils.synthesizeKey("VK_TAB", {}, tooltipWin);
  const checkbox = tooltipItems[0].querySelector("input[type=checkbox]");
  is(tooltipDoc.activeElement, checkbox, "Focus was moved to the checkbox");
  is(
    checkbox.getAttribute("aria-label"),
    "Enable “click” event listener",
    "Checkbox has expected label"
  );

  info("Press Tab key to move to next event listener item");
  EventUtils.synthesizeKey("VK_TAB", {}, tooltipWin);
  is(
    tooltipDoc.activeElement,
    tooltipItems[1].querySelector("button.theme-twisty"),
    "Focus was moved to next event listener item twisty button"
  );

  info("Press Escape key to close popup");
  const onTooltipHidden = tooltip.once("hidden");
  EventUtils.sendKey("ESCAPE", inspector.toolbox.win);
  await onTooltipHidden;
  ok(true, "The tooltip is hidden");
  await waitFor(
    () => eventTooltipBadge.getAttribute("aria-pressed") === "false"
  );
  ok(true, "The event tooltip badge is not pressed anymore");

  // wait for a bit to check the split console wasn't opened
  await wait(500);
  ok(!inspector.toolbox.splitConsole, "Split console is now hidden.");
});
