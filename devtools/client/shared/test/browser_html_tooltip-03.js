/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip autofocus configuration option.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip-03.xul";

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

let useXulWrapper;

add_task(async function() {
  await addTab("about:blank");
  const [, , doc] = await createHost("bottom", TEST_URI);

  info("Run tests for a Tooltip without using a XUL panel");
  useXulWrapper = false;
  await runTests(doc);

  info("Run tests for a Tooltip with a XUL panel");
  useXulWrapper = true;
  await runTests(doc);
});

async function runTests(doc) {
  await testNoAutoFocus(doc);
  await testAutoFocus(doc);
  await testAutoFocusPreservesFocusChange(doc);
}

async function testNoAutoFocus(doc) {
  await focusNode(doc, "#box4-input");
  ok(doc.activeElement.closest("#box4-input"), "Focus is in the #box4-input");

  info("Test a tooltip without autofocus will not take focus");
  const tooltip = await createTooltip(doc, false);

  await showTooltip(tooltip, doc.getElementById("box1"));
  ok(doc.activeElement.closest("#box4-input"), "Focus is still in the #box4-input");

  await hideTooltip(tooltip);
  await blurNode(doc, "#box4-input");

  tooltip.destroy();
}

async function testAutoFocus(doc) {
  await focusNode(doc, "#box4-input");
  ok(doc.activeElement.closest("#box4-input"), "Focus is in the #box4-input");

  info("Test autofocus tooltip takes focus when displayed, " +
    "and restores the focus when hidden");
  const tooltip = await createTooltip(doc, true);

  await showTooltip(tooltip, doc.getElementById("box1"));
  ok(doc.activeElement.closest(".tooltip-content"), "Focus is in the tooltip");

  await hideTooltip(tooltip);
  ok(doc.activeElement.closest("#box4-input"), "Focus is in the #box4-input");

  info("Blur the textbox before moving to the next test to reset the state.");
  await blurNode(doc, "#box4-input");

  tooltip.destroy();
}

async function testAutoFocusPreservesFocusChange(doc) {
  await focusNode(doc, "#box4-input");
  ok(doc.activeElement.closest("#box4-input"), "Focus is still in the #box3-input");

  info("Test autofocus tooltip takes focus when displayed, " +
    "but does not try to restore the active element if it is not focused when hidden");
  const tooltip = await createTooltip(doc, true);

  await showTooltip(tooltip, doc.getElementById("box1"));
  ok(doc.activeElement.closest(".tooltip-content"), "Focus is in the tooltip");

  info("Move the focus to #box3-input while the tooltip is displayed");
  await focusNode(doc, "#box3-input");
  ok(doc.activeElement.closest("#box3-input"), "Focus moved to the #box3-input");

  await hideTooltip(tooltip);
  ok(doc.activeElement.closest("#box3-input"), "Focus is still in the #box3-input");

  info("Blur the textbox before moving to the next test to reset the state.");
  await blurNode(doc, "#box3-input");

  tooltip.destroy();
}

/**
 * Fpcus the node corresponding to the provided selector in the provided document. Returns
 * a promise that will resolve when receiving the focus event on the node.
 */
function focusNode(doc, selector) {
  const node = doc.querySelector(selector);
  const onFocus = once(node, "focus");
  node.focus();
  return onFocus;
}

/**
 * Blur the node corresponding to the provided selector in the provided document. Returns
 * a promise that will resolve when receiving the blur event on the node.
 */
function blurNode(doc, selector) {
  const node = doc.querySelector(selector);
  const onBlur = once(node, "blur");
  node.blur();
  return onBlur;
}

/**
 * Create an HTMLTooltip instance with the provided autofocus setting.
 *
 * @param {Document} doc
 *        Document in which the tooltip should be created
 * @param {Boolean} autofocus
 * @return {Promise} promise that will resolve the HTMLTooltip instance created when the
 *         tooltip content will be ready.
 */
function createTooltip(doc, autofocus) {
  const tooltip = new HTMLTooltip(doc, {autofocus, useXulWrapper});
  const div = doc.createElementNS(HTML_NS, "div");
  div.classList.add("tooltip-content");
  div.style.height = "50px";

  const input = doc.createElementNS(HTML_NS, "input");
  input.setAttribute("type", "text");
  div.appendChild(input);

  tooltip.setContent(div, {width: 150, height: 50});
  return tooltip;
}
