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
        "<frameset cols='50%,50%'>",
          "<frame src='",
          ["data:text/html,",
           "<!DOCTYPE html>",
           "<html>",
           "<body style='margin: 0;'>",
           "<div id='test-div' style='width: 123px; height: 456px;'></div>",
           "</body>",
           "</html>"
          ].join(""),
          "' />",
          "<frame src='",
          ["data:text/html,",
           "<!DOCTYPE html>",
           "<html>",
           "<body style='margin: 0;'>",
           "<span></span>",
           "</body>",
           "</html>"
          ].join(""),
          "' />",
        "</frameset>",
        "<iframe src='",
        ["data:text/html,",
         "<!DOCTYPE html>",
         "<html>",
         "<body>",
         "<span></span>",
         "</body>",
         "</html>"
        ].join(""),
        "'></iframe>",
        "<frame src='",
        ["data:text/html,",
         "<!DOCTYPE html>",
         "<html>",
         "<body style='margin: 0;'>",
         "<span></span>",
         "</body>",
         "</html>"
        ].join(""),
        "' />",
        "<frame src='",
        ["data:text/html,",
         "<!DOCTYPE html>",
         "<html>",
         "<body style='margin: 0;'>",
         "<iframe src='",
         ["data:text/html,",
          "<!DOCTYPE html>",
          "<html>",
          "<body>",
          "<div></div>",
          "</body>",
          "</html>"
         ].join(""),
         "'></iframe>",
         "</body>",
         "</html>"
        ].join(""),
        "' />",
      "</body>",
    "</html>"
  ].join(""));

  gBrowser.parentNode.appendChild(iframe);
}

function test() {
  waitForExplicitFinish();
  ok(TiltUtils, "The TiltUtils object doesn't exist.");

  let dom = TiltUtils.DOM;
  ok(dom, "The TiltUtils.DOM wasn't found.");

  init(function(iframe) {
    let cwDimensions = dom.getContentWindowDimensions(iframe.contentWindow);

    is(cwDimensions.width - iframe.contentWindow.scrollMaxX,
      iframe.contentWindow.innerWidth,
      "The content window width wasn't calculated correctly.");
    is(cwDimensions.height - iframe.contentWindow.scrollMaxY,
      iframe.contentWindow.innerHeight,
      "The content window height wasn't calculated correctly.");

    let nodeCoordinates = LayoutHelpers.getRect(
      iframe.contentDocument.getElementById("test-div"), iframe.contentWindow);

    let frameOffset = LayoutHelpers.getIframeContentOffset(iframe);
    let frameRect = iframe.getBoundingClientRect();

    is(nodeCoordinates.top, frameRect.top + frameOffset[0],
      "The node coordinates top value wasn't calculated correctly.");
    is(nodeCoordinates.left, frameRect.left + frameOffset[1],
      "The node coordinates left value wasn't calculated correctly.");
    is(nodeCoordinates.width, 123,
      "The node coordinates width value wasn't calculated correctly.");
    is(nodeCoordinates.height, 456,
      "The node coordinates height value wasn't calculated correctly.");


    let store = dom.traverse(iframe.contentWindow);

    let expected = [
      { name: "html",   depth: 0 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "head",   depth: 1 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "body",   depth: 1 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "div",    depth: 2 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "span",   depth: 2 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "iframe", depth: 2 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "span",   depth: 2 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "iframe", depth: 2 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "html",   depth: 3 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "html",   depth: 3 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "head",   depth: 4 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "body",   depth: 4 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "head",   depth: 4 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "body",   depth: 4 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "span",   depth: 5 * STACK_THICKNESS, thickness: STACK_THICKNESS },
      { name: "div",    depth: 5 * STACK_THICKNESS, thickness: STACK_THICKNESS },
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
