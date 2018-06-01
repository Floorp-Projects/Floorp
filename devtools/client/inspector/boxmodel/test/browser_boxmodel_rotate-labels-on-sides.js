/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that longer values are rotated on the side

const res1 = [
  {selector: ".boxmodel-margin.boxmodel-top > span", value: 30},
  {selector: ".boxmodel-margin.boxmodel-left > span", value: "auto"},
  {selector: ".boxmodel-margin.boxmodel-bottom > span", value: 30},
  {selector: ".boxmodel-margin.boxmodel-right > span", value: "auto"},
  {selector: ".boxmodel-padding.boxmodel-top > span", value: 20},
  {selector: ".boxmodel-padding.boxmodel-left > span", value: 2000000},
  {selector: ".boxmodel-padding.boxmodel-bottom > span", value: 20},
  {selector: ".boxmodel-padding.boxmodel-right > span", value: 20},
  {selector: ".boxmodel-border.boxmodel-top > span", value: 10},
  {selector: ".boxmodel-border.boxmodel-left > span", value: 10},
  {selector: ".boxmodel-border.boxmodel-bottom > span", value: 10},
  {selector: ".boxmodel-border.boxmodel-right > span", value: 10},
];

const TEST_URI = encodeURIComponent([
  "<style>",
  "div { border:10px solid black; padding: 20px 20px 20px 2000000px; " +
  "margin: 30px auto; }",
  "</style>",
  "<div></div>"
].join(""));
const LONG_TEXT_ROTATE_LIMIT = 3;

add_task(async function() {
  await addTab("data:text/html," + TEST_URI);
  const {inspector, boxmodel} = await openLayoutView();
  await selectNode("div", inspector);

  for (let i = 0; i < res1.length; i++) {
    const elt = boxmodel.document.querySelector(res1[i].selector);
    const isLong = elt.textContent.length > LONG_TEXT_ROTATE_LIMIT;
    const classList = elt.parentNode.classList;
    const canBeRotated = classList.contains("boxmodel-left") ||
                       classList.contains("boxmodel-right");
    const isRotated = classList.contains("boxmodel-rotate");

    is(canBeRotated && isLong,
      isRotated, res1[i].selector + " correctly rotated.");
  }
});
