/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip show & hide methods.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip.xul";

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

let useXulWrapper;

function getTooltipContent(doc) {
  const div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "50px";
  div.style.boxSizing = "border-box";
  div.textContent = "tooltip";
  return div;
}

add_task(async function() {
  const [,, doc] = await createHost("bottom", TEST_URI);

  info("Run tests for a Tooltip without using a XUL panel");
  useXulWrapper = false;
  await runTests(doc);

  info("Run tests for a Tooltip with a XUL panel");
  useXulWrapper = true;
  await runTests(doc);
});

async function runTests(doc) {
  await addTab("about:blank");
  const tooltip = new HTMLTooltip(doc, {useXulWrapper});

  info("Set tooltip content");
  tooltip.setContent(getTooltipContent(doc), {width: 100, height: 50});

  is(tooltip.isVisible(), false, "Tooltip is not visible");

  info("Show the tooltip and check the expected events are fired.");

  let shown = 0;
  tooltip.on("shown", () => shown++);

  const onShown = tooltip.once("shown");
  tooltip.show(doc.getElementById("box1"));

  await onShown;
  is(shown, 1, "Event shown was fired once");

  await waitForReflow(tooltip);
  is(tooltip.isVisible(), true, "Tooltip is visible");

  info("Hide the tooltip and check the expected events are fired.");

  let hidden = 0;
  tooltip.on("hidden", () => hidden++);

  const onPopupHidden = tooltip.once("hidden");
  tooltip.hide();

  await onPopupHidden;
  is(hidden, 1, "Event hidden was fired once");

  await waitForReflow(tooltip);
  is(tooltip.isVisible(), false, "Tooltip is not visible");

  tooltip.destroy();
}
