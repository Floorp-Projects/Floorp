/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

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
      "<head>",
        "<style>",
        "</style>",
        "<script>",
        "</script>",
      "</head>",
      "<body style='margin: 0;'>",
        "<div style='margin-top: 98px;" +
                    "margin-left: 76px;" +
                    "width: 123px;" +
                    "height: 456px;' id='test-div'>",
          "<span></span>",
        "</div>",
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

    is(nodeCoordinates.top, frameRect.top + frameOffset[0] + 98,
      "The node coordinates top value wasn't calculated correctly.");
    is(nodeCoordinates.left, frameRect.left + frameOffset[1] + 76,
      "The node coordinates left value wasn't calculated correctly.");
    is(nodeCoordinates.width, 123,
      "The node coordinates width value wasn't calculated correctly.");
    is(nodeCoordinates.height, 456,
      "The node coordinates height value wasn't calculated correctly.");


    let store = dom.traverse(iframe.contentWindow);

    is(store.nodes.length, 7,
      "The traverse() function didn't walk the correct number of nodes.");
    is(store.info.length, 7,
      "The traverse() function didn't examine the correct number of nodes.");
    is(store.info[0].name, "html",
      "the 1st traversed node isn't the expected one.");
    is(store.info[0].depth, 0,
      "the 1st traversed node doesn't have the expected depth.");
    is(store.info[1].name, "head",
      "the 2nd traversed node isn't the expected one.");
    is(store.info[1].depth, 1,
      "the 2nd traversed node doesn't have the expected depth.");
    is(store.info[2].name, "body",
      "the 3rd traversed node isn't the expected one.");
    is(store.info[2].depth, 1,
      "the 3rd traversed node doesn't have the expected depth.");
    is(store.info[3].name, "style",
      "the 4th traversed node isn't the expected one.");
    is(store.info[3].depth, 2,
      "the 4th traversed node doesn't have the expected depth.");
    is(store.info[4].name, "script",
      "the 5th traversed node isn't the expected one.");
    is(store.info[4].depth, 2,
      "the 5th traversed node doesn't have the expected depth.");
    is(store.info[5].name, "div",
      "the 6th traversed node isn't the expected one.");
    is(store.info[5].depth, 2,
      "the 6th traversed node doesn't have the expected depth.");
    is(store.info[6].name, "span",
      "the 7th traversed node isn't the expected one.");
    is(store.info[6].depth, 3,
      "the 7th traversed node doesn't have the expected depth.");
  });
}
