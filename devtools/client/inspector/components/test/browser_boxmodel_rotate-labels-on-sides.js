/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that longer values are rotated on the side

const res1 = [
  {selector: ".old-boxmodel-margin.old-boxmodel-top > span", value: 30},
  {selector: ".old-boxmodel-margin.old-boxmodel-left > span", value: "auto"},
  {selector: ".old-boxmodel-margin.old-boxmodel-bottom > span", value: 30},
  {selector: ".old-boxmodel-margin.old-boxmodel-right > span", value: "auto"},
  {selector: ".old-boxmodel-padding.old-boxmodel-top > span", value: 20},
  {selector: ".old-boxmodel-padding.old-boxmodel-left > span", value: 2000000},
  {selector: ".old-boxmodel-padding.old-boxmodel-bottom > span", value: 20},
  {selector: ".old-boxmodel-padding.old-boxmodel-right > span", value: 20},
  {selector: ".old-boxmodel-border.old-boxmodel-top > span", value: 10},
  {selector: ".old-boxmodel-border.old-boxmodel-left > span", value: 10},
  {selector: ".old-boxmodel-border.old-boxmodel-bottom > span", value: 10},
  {selector: ".old-boxmodel-border.old-boxmodel-right > span", value: 10},
];

const TEST_URI = encodeURIComponent([
  "<style>",
  "div { border:10px solid black; padding: 20px 20px 20px 2000000px; " +
  "margin: 30px auto; }",
  "</style>",
  "<div></div>"
].join(""));
const LONG_TEXT_ROTATE_LIMIT = 3;

add_task(function* () {
  yield addTab("data:text/html," + TEST_URI);
  let {inspector, view} = yield openBoxModelView();
  yield selectNode("div", inspector);

  for (let i = 0; i < res1.length; i++) {
    let elt = view.doc.querySelector(res1[i].selector);
    let isLong = elt.textContent.length > LONG_TEXT_ROTATE_LIMIT;
    let classList = elt.parentNode.classList;
    let canBeRotated = classList.contains("old-boxmodel-left") ||
                       classList.contains("old-boxmodel-right");
    let isRotated = classList.contains("old-boxmodel-rotate");

    is(canBeRotated && isLong,
      isRotated, res1[i].selector + " correctly rotated.");
  }
});
