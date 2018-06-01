/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that properties can be selected and copied from the computed view.

const TEST_URI = `<div style="text-align:left;width:25px;">Hello world</div>`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openComputedView();
  await selectNode("div", inspector);

  let expectedPattern = "text-align: left;[\\r\\n]+" +
                        "width: 25px;[\\r\\n]*";
  await copyAllAndCheckClipboard(view, expectedPattern);

  info("Testing expand then select all copy");

  expectedPattern = "text-align: left;[\\r\\n]+" +
                    "element[\\r\\n]+" +
                    "this.style[\\r\\n]+" +
                    "left[\\r\\n]+" +
                    "width: 25px;[\\r\\n]+" +
                    "element[\\r\\n]+" +
                    "this.style[\\r\\n]+" +
                    "25px[\\r\\n]*";

  info("Expanding computed view properties");
  await expandComputedViewPropertyByIndex(view, 0);
  await expandComputedViewPropertyByIndex(view, 1);

  await copyAllAndCheckClipboard(view, expectedPattern);
});
