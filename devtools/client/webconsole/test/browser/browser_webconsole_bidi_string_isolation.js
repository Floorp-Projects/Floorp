/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<!DOCTYPE html>Bidi strings";
const rtlOverride = "\u202e";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const browser = gBrowser.selectedBrowser;

  /* eslint-disable-next-line no-shadow */
  await SpecialPowers.spawn(browser, [rtlOverride], rtlOverride => {
    const { console } = content.wrappedJSObject;

    console.log(Symbol(rtlOverride + "msg01"));
    console.log([rtlOverride + "msg02"]);
    console.log({ p: rtlOverride + "msg03" });
    console.log({ [rtlOverride + "msg04"]: null });
    console.log(new Set([rtlOverride + "msg05"]));
    console.log(new Map([[rtlOverride + "msg06", null]]));
    console.log(new Map([[null, rtlOverride + "msg07"]]));

    const parser = content.document.createElement("div");
    parser.innerHTML = `
      <div data-test="${rtlOverride}msg08"></div>
      <div data-${rtlOverride}="msg09"></div>
      <div-${rtlOverride} msg10></div-${rtlOverride}>
    `;
    for (const child of parser.children) {
      console.log(child);
    }
  });

  const texts = [
    `Symbol("${rtlOverride}msg01")`,
    `Array [ "${rtlOverride}msg02" ]`,
    `Object { p: "${rtlOverride}msg03" }`,
    `Object { "${rtlOverride}msg04": null }`,
    `Set [ "${rtlOverride}msg05" ]`,
    `Map { "${rtlOverride}msg06" → null }`,
    `Map { null → "${rtlOverride}msg07" }`,
    `<div data-test="${rtlOverride}msg08">`,
    `<div data-${rtlOverride}="msg09">`,
    `<div-${rtlOverride} msg10="">`,
  ];
  for (let i = 0; i < texts.length; ++i) {
    const msgId = "msg" + String(i + 1).padStart(2, "0");
    const message = await waitFor(() => findConsoleAPIMessage(hud, msgId));
    const objectBox = message.querySelector(".objectBox");
    is(objectBox.textContent, texts[i], "Should have all the relevant text");
    checkRects(objectBox);
  }
});

function getBoundingClientRect(node) {
  if (node.nodeType === Node.ELEMENT_NODE) {
    return node.getBoundingClientRect();
  }
  // There is no Node.getBoundingClientRect, use a Range instead.
  const range = document.createRange();
  range.selectNode(node);
  return range.getBoundingClientRect();
}

/**
 * The console prints data build from external strings. They can contain
 * characters that change the directionality of the text. For example, RTL
 * characters will flow right to left. However, this should be isolated to
 * prevent one string from mangling how another one is rendered.
 * This function uses getBoundingClientRect() to check that the nodes, as a
 * whole, flow LTR (even if the characters in the node flow RTL).
 * The bidi algorithm happens at layout time, so we need to check the rects,
 * DOM operations like textContent would be useless.
 */
function checkRects(node, parentRect = getBoundingClientRect(node)) {
  let prevRect;
  for (const child of node.childNodes) {
    const rect = getBoundingClientRect(child);
    ok(rect.x >= parentRect.x, "Rect should start inside parent");
    ok(
      rect.x + rect.width <= parentRect.x + parentRect.width,
      "Rect should end inside parent"
    );
    if (prevRect) {
      ok(
        rect.x >= prevRect.x + prevRect.width,
        "Rect should start after previous one"
      );
    }
    prevRect = rect;
    checkRects(child, rect);
  }
}
