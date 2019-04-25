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

add_task(async function() {
  await addTab("about:blank");
  const [,, doc] = await createHost("bottom", TEST_URI);

  info("Run tests for a Tooltip without using a XUL panel");
  useXulWrapper = false;
  await runTests(doc);

  info("Run tests for a Tooltip with a XUL panel");
  useXulWrapper = true;
  await runTests(doc);
});

async function runTests(doc) {
  await testClickInTooltipContent(doc);
  await testConsumeOutsideClicksFalse(doc);
  await testConsumeOutsideClicksTrue(doc);
  await testConsumeWithRightClick(doc);
  await testClickInOuterIframe(doc);
  await testClickInInnerIframe(doc);
}

async function testClickInTooltipContent(doc) {
  info("Test a tooltip is not closed when clicking inside itself");

  const tooltip = new HTMLTooltip(doc, {useXulWrapper});
  tooltip.panel.appendChild(getTooltipContent(doc));
  tooltip.setContentSize({width: 100, height: 50});
  await showTooltip(tooltip, doc.getElementById("box1"));

  const onTooltipContainerClick = once(tooltip.container, "click");
  EventUtils.synthesizeMouseAtCenter(tooltip.container, {}, doc.defaultView);
  await onTooltipContainerClick;
  is(tooltip.isVisible(), true, "Tooltip is still visible");

  tooltip.destroy();
}

async function testConsumeOutsideClicksFalse(doc) {
  info("Test closing a tooltip via click with consumeOutsideClicks: false");
  const box4 = doc.getElementById("box4");

  const tooltip = new HTMLTooltip(doc, {consumeOutsideClicks: false, useXulWrapper});
  tooltip.panel.appendChild(getTooltipContent(doc));
  tooltip.setContentSize({width: 100, height: 50});
  await showTooltip(tooltip, doc.getElementById("box1"));

  const onBox4Clicked = once(box4, "click");
  const onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouseAtCenter(box4, {}, doc.defaultView);
  await onHidden;
  await onBox4Clicked;

  is(tooltip.isVisible(), false, "Tooltip is hidden");

  tooltip.destroy();
}

async function testConsumeOutsideClicksTrue(doc) {
  info("Test closing a tooltip via click with consumeOutsideClicks: true");
  const box4 = doc.getElementById("box4");

  // Count clicks on box4
  let box4clicks = 0;
  box4.addEventListener("click", () => box4clicks++);

  const tooltip = new HTMLTooltip(doc, {consumeOutsideClicks: true, useXulWrapper});
  tooltip.panel.appendChild(getTooltipContent(doc));
  tooltip.setContentSize({width: 100, height: 50});
  await showTooltip(tooltip, doc.getElementById("box1"));

  const onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouseAtCenter(box4, {}, doc.defaultView);
  await onHidden;

  is(box4clicks, 0, "box4 catched no click event");
  is(tooltip.isVisible(), false, "Tooltip is hidden");

  tooltip.destroy();
}

async function testConsumeWithRightClick(doc) {
  info("Test closing a tooltip with a right-click, with consumeOutsideClicks: true");
  const box4 = doc.getElementById("box4");

  const tooltip = new HTMLTooltip(doc, {consumeOutsideClicks: true, useXulWrapper});
  tooltip.panel.appendChild(getTooltipContent(doc));
  tooltip.setContentSize({width: 100, height: 50});
  await showTooltip(tooltip, doc.getElementById("box1"));

  // Only left-click events should be consumed, so we expect to catch a click when using
  // {button: 2}, which simulates a right-click.
  info("Right click on box4, expect tooltip to be hidden, event should not be consumed");
  const onBox4Clicked = once(box4, "click");
  const onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouseAtCenter(box4, {button: 2}, doc.defaultView);
  await onHidden;
  await onBox4Clicked;

  is(tooltip.isVisible(), false, "Tooltip is hidden");

  tooltip.destroy();
}

async function testClickInOuterIframe(doc) {
  info("Test clicking an iframe outside of the tooltip closes the tooltip");
  const frame = doc.getElementById("frame");

  const tooltip = new HTMLTooltip(doc, {useXulWrapper});
  tooltip.panel.appendChild(getTooltipContent(doc));
  tooltip.setContentSize({width: 100, height: 50});
  await showTooltip(tooltip, doc.getElementById("box1"));

  const onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouseAtCenter(frame, {}, doc.defaultView);
  await onHidden;

  is(tooltip.isVisible(), false, "Tooltip is hidden");
  tooltip.destroy();
}

async function testClickInInnerIframe(doc) {
  info("Test clicking an iframe inside the tooltip content does not close the tooltip");

  const tooltip = new HTMLTooltip(doc, {consumeOutsideClicks: false, useXulWrapper});

  const iframe = doc.createElementNS(HTML_NS, "iframe");
  iframe.style.width = "100px";
  iframe.style.height = "50px";

  tooltip.panel.appendChild(iframe);

  const onFrameLoad = new Promise(r => {
    iframe.addEventListener("load", r, true);
  });
  iframe.srcdoc = "<div id=test style='height:50px;'></div>";
  await onFrameLoad;

  tooltip.setContentSize({width: 100, height: 50});
  await showTooltip(tooltip, doc.getElementById("box1"));

  const target = iframe.contentWindow.document.documentElement;
  const onTooltipContainerClick = once(target, "click");

  EventUtils.synthesizeMouseAtCenter(target, {}, target.ownerDocument.defaultView);
  await onTooltipContainerClick;

  is(tooltip.isVisible(), true, "Tooltip is still visible");

  tooltip.destroy();
}

function getTooltipContent(doc) {
  const div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "50px";
  div.style.boxSizing = "border-box";
  div.textContent = "tooltip";
  return div;
}
