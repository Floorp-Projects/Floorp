/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */
"use strict";

/**
 * Test the HTMLTooltip is closed when clicking outside of its container.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip-02.xul";

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

let useXulWrapper;

add_task(function* () {
  yield addTab("about:blank");
  let [,, doc] = yield createHost("bottom", TEST_URI);

  info("Run tests for a Tooltip without using a XUL panel");
  useXulWrapper = false;
  yield runTests(doc);

  info("Run tests for a Tooltip with a XUL panel");
  useXulWrapper = true;
  yield runTests(doc);
});

function* runTests(doc) {
  yield testClickInTooltipContent(doc);
  yield testConsumeOutsideClicksFalse(doc);
  yield testConsumeOutsideClicksTrue(doc);
  yield testConsumeWithRightClick(doc);
  yield testClickInOuterIframe(doc);
  yield testClickInInnerIframe(doc);
}

function* testClickInTooltipContent(doc) {
  info("Test a tooltip is not closed when clicking inside itself");

  let tooltip = new HTMLTooltip(doc, {useXulWrapper});
  tooltip.setContent(getTooltipContent(doc), {width: 100, height: 50});
  yield showTooltip(tooltip, doc.getElementById("box1"));

  let onTooltipContainerClick = once(tooltip.container, "click");
  EventUtils.synthesizeMouseAtCenter(tooltip.container, {}, doc.defaultView);
  yield onTooltipContainerClick;
  is(tooltip.isVisible(), true, "Tooltip is still visible");

  tooltip.destroy();
}

function* testConsumeOutsideClicksFalse(doc) {
  info("Test closing a tooltip via click with consumeOutsideClicks: false");
  let box4 = doc.getElementById("box4");

  let tooltip = new HTMLTooltip(doc, {consumeOutsideClicks: false, useXulWrapper});
  tooltip.setContent(getTooltipContent(doc), {width: 100, height: 50});
  yield showTooltip(tooltip, doc.getElementById("box1"));

  let onBox4Clicked = once(box4, "click");
  let onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouseAtCenter(box4, {}, doc.defaultView);
  yield onHidden;
  yield onBox4Clicked;

  is(tooltip.isVisible(), false, "Tooltip is hidden");

  tooltip.destroy();
}

function* testConsumeOutsideClicksTrue(doc) {
  info("Test closing a tooltip via click with consumeOutsideClicks: true");
  let box4 = doc.getElementById("box4");

  // Count clicks on box4
  let box4clicks = 0;
  box4.addEventListener("click", () => box4clicks++);

  let tooltip = new HTMLTooltip(doc, {consumeOutsideClicks: true, useXulWrapper});
  tooltip.setContent(getTooltipContent(doc), {width: 100, height: 50});
  yield showTooltip(tooltip, doc.getElementById("box1"));

  let onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouseAtCenter(box4, {}, doc.defaultView);
  yield onHidden;

  is(box4clicks, 0, "box4 catched no click event");
  is(tooltip.isVisible(), false, "Tooltip is hidden");

  tooltip.destroy();
}

function* testConsumeWithRightClick(doc) {
  info("Test closing a tooltip with a right-click, with consumeOutsideClicks: true");
  let box4 = doc.getElementById("box4");

  let tooltip = new HTMLTooltip(doc, {consumeOutsideClicks: true, useXulWrapper});
  tooltip.setContent(getTooltipContent(doc), {width: 100, height: 50});
  yield showTooltip(tooltip, doc.getElementById("box1"));

  // Only left-click events should be consumed, so we expect to catch a click when using
  // {button: 2}, which simulates a right-click.
  info("Right click on box4, expect tooltip to be hidden, event should not be consumed");
  let onBox4Clicked = once(box4, "click");
  let onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouseAtCenter(box4, {button: 2}, doc.defaultView);
  yield onHidden;
  yield onBox4Clicked;

  is(tooltip.isVisible(), false, "Tooltip is hidden");

  tooltip.destroy();
}

function* testClickInOuterIframe(doc) {
  info("Test clicking an iframe outside of the tooltip closes the tooltip");
  let frame = doc.getElementById("frame");

  let tooltip = new HTMLTooltip(doc, {useXulWrapper});
  tooltip.setContent(getTooltipContent(doc), {width: 100, height: 50});
  yield showTooltip(tooltip, doc.getElementById("box1"));

  let onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouseAtCenter(frame, {}, doc.defaultView);
  yield onHidden;

  is(tooltip.isVisible(), false, "Tooltip is hidden");
  tooltip.destroy();
}

function* testClickInInnerIframe(doc) {
  info("Test clicking an iframe inside the tooltip content does not close the tooltip");

  let tooltip = new HTMLTooltip(doc, {consumeOutsideClicks: false, useXulWrapper});

  let iframe = doc.createElementNS(HTML_NS, "iframe");
  iframe.style.width = "100px";
  iframe.style.height = "50px";
  tooltip.setContent(iframe, {width: 100, height: 50});
  yield showTooltip(tooltip, doc.getElementById("box1"));

  let onTooltipContainerClick = once(tooltip.container, "click");
  EventUtils.synthesizeMouseAtCenter(tooltip.container, {}, doc.defaultView);
  yield onTooltipContainerClick;

  is(tooltip.isVisible(), true, "Tooltip is still visible");

  tooltip.destroy();
}

function getTooltipContent(doc) {
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "50px";
  div.style.boxSizing = "border-box";
  div.textContent = "tooltip";
  return div;
}
