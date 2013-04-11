/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const STACK_THICKNESS = 15;

function init(callback) {
  let iframe = gBrowser.ownerDocument.createElement("iframe");

  iframe.addEventListener("load", function onLoad() {
    iframe.removeEventListener("load", onLoad, true);
    callback(iframe);

    gBrowser.parentNode.removeChild(iframe);
    finish();
  }, true);

  iframe.setAttribute("src", ["data:text/html,",
    "<!DOCTYPE html>",
    "<html>",
      "<body style='margin: 0;'>",
        "<div>",
          "<p>Foo</p>",
          "<div>",
            "<span>Bar</span>",
          "</div>",
          "<div></div>",
        "</div>",
      "</body>",
    "</html>"
  ].join(""));

  gBrowser.parentNode.appendChild(iframe);
}

function nodeCallback(aContentWindow, aNode, aParentPosition) {
  let coord = TiltUtils.DOM.getNodePosition(aContentWindow, aNode, aParentPosition);

  if (aNode.localName != "div")
    coord.thickness = 0;

  if (aNode.localName == "span")
    coord.depth += STACK_THICKNESS;

  return coord;
}

function test() {
  waitForExplicitFinish();
  ok(TiltUtils, "The TiltUtils object doesn't exist.");

  let dom = TiltUtils.DOM;
  ok(dom, "The TiltUtils.DOM wasn't found.");

  init(function(iframe) {
    let store = dom.traverse(iframe.contentWindow, {
      nodeCallback: nodeCallback
    });

    let expected = [
      { name: "html",   depth: 0 * STACK_THICKNESS, thickness: 0 },
      { name: "head",   depth: 0 * STACK_THICKNESS, thickness: 0 },
      { name: "body",   depth: 0 * STACK_THICKNESS, thickness: 0 },
      { name: "div",    depth: 0 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "p",      depth: 1 * STACK_THICKNESS, thickness: 0 },
      { name: "div",    depth: 1 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "div",    depth: 1 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "span",   depth: 3 * STACK_THICKNESS, thickness: 0 },
    ];

    is(store.nodes.length, expected.length,
      "The traverse() function didn't walk the correct number of nodes.");
    is(store.info.length, expected.length,
      "The traverse() function didn't examine the correct number of nodes.");

    for (let i = 0; i < expected.length; i++) {
      is(store.info[i].name, expected[i].name,
        "traversed node " + (i + 1) + " isn't the expected one.");
      is(store.info[i].coord.depth, expected[i].depth,
        "traversed node " + (i + 1) + " doesn't have the expected depth.");
      is(store.info[i].coord.thickness, expected[i].thickness,
        "traversed node " + (i + 1) + " doesn't have the expected thickness.");
    }
  });
}
