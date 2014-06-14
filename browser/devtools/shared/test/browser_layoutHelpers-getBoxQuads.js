/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that LayoutHelpers.getAdjustedQuads works properly in a variety of use
// cases including iframes, scroll and zoom

const {utils: Cu} = Components;
const {LayoutHelpers} = Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm", {});

const TEST_URI = TEST_URI_ROOT + "browser_layoutHelpers-getBoxQuads.html";

function test() {
  addTab(TEST_URI, function(browser, tab) {
    let doc = browser.contentDocument;
    let win = doc.defaultView;

    info("Creating a new LayoutHelpers instance for the test window");
    let helper = new LayoutHelpers(win);
    ok(helper.getAdjustedQuads, "getAdjustedQuads is defined");

    info("Running tests");

    returnsTheRightDataStructure(doc, helper);
    returnsNullForMissingNode(doc, helper);
    returnsNullForHiddenNodes(doc, helper);
    defaultsToBorderBoxIfNoneProvided(doc, helper);
    returnsLikeGetBoxQuadsInSimpleCase(doc, helper);
    takesIframesOffsetsIntoAccount(doc, helper);
    takesScrollingIntoAccount(doc, helper);
    takesZoomIntoAccount(doc, helper);

    gBrowser.removeCurrentTab();
    finish();
  });
}

function returnsTheRightDataStructure(doc, helper) {
  info("Checks that the returned data contains bounds and 4 points");

  let node = doc.querySelector("body");
  let res = helper.getAdjustedQuads(node, "content");

  ok("bounds" in res, "The returned data has a bounds property");
  ok("p1" in res, "The returned data has a p1 property");
  ok("p2" in res, "The returned data has a p2 property");
  ok("p3" in res, "The returned data has a p3 property");
  ok("p4" in res, "The returned data has a p4 property");

  for (let boundProp of
    ["bottom", "top", "right", "left", "width", "height", "x", "y"]) {
    ok(boundProp in res.bounds, "The bounds has a " + boundProp + " property");
  }

  for (let point of ["p1", "p2", "p3", "p4"]) {
    for (let pointProp of ["x", "y", "z", "w"]) {
      ok(pointProp in res[point], point + " has a " + pointProp + " property");
    }
  }
}

function returnsNullForMissingNode(doc, helper) {
  info("Checks that null is returned for invalid nodes");

  for (let input of [null, undefined, "", 0]) {
    ok(helper.getAdjustedQuads(input) === null, "null is returned for input " +
      input);
  }
}

function returnsNullForHiddenNodes(doc, helper) {
  info("Checks that null is returned for nodes that aren't rendered");

  let style = doc.querySelector("#styles");
  ok(helper.getAdjustedQuads(style) === null,
    "null is returned for a <style> node");

  let hidden = doc.querySelector("#hidden-node");
  ok(helper.getAdjustedQuads(hidden) === null,
    "null is returned for a hidden node");
}

function defaultsToBorderBoxIfNoneProvided(doc, helper) {
  info("Checks that if no boxtype is passed, then border is the default one");

  let node = doc.querySelector("#simple-node-with-margin-padding-border");
  let withBoxType = helper.getAdjustedQuads(node, "border");
  let withoutBoxType = helper.getAdjustedQuads(node);

  for (let boundProp of
    ["bottom", "top", "right", "left", "width", "height", "x", "y"]) {
    is(withBoxType.bounds[boundProp], withoutBoxType.bounds[boundProp],
      boundProp + " bound is equal with or without the border box type");
  }

  for (let point of ["p1", "p2", "p3", "p4"]) {
    for (let pointProp of ["x", "y", "z", "w"]) {
      is(withBoxType[point][pointProp], withoutBoxType[point][pointProp],
        point + "." + pointProp +
        " is equal with or without the border box type");
    }
  }
}

function returnsLikeGetBoxQuadsInSimpleCase(doc, helper) {
  info("Checks that for an element in the main frame, without scroll nor zoom" +
    "that the returned value is similar to the returned value of getBoxQuads");

  let node = doc.querySelector("#simple-node-with-margin-padding-border");

  for (let region of ["content", "padding", "border", "margin"]) {
    let expected = node.getBoxQuads({
      box: region
    })[0];
    let actual = helper.getAdjustedQuads(node, region);

    for (let boundProp of
      ["bottom", "top", "right", "left", "width", "height", "x", "y"]) {
      is(actual.bounds[boundProp], expected.bounds[boundProp],
        boundProp + " bound is equal to the one returned by getBoxQuads for " +
        region + " box");
    }

    for (let point of ["p1", "p2", "p3", "p4"]) {
      for (let pointProp of ["x", "y", "z", "w"]) {
        is(actual[point][pointProp], expected[point][pointProp],
          point + "." + pointProp +
          " is equal to the one returned by getBoxQuads for " + region + " box");
      }
    }
  }
}

function takesIframesOffsetsIntoAccount(doc, helper) {
  info("Checks that the quad returned for a node inside iframes that have " +
    "margins takes those offsets into account");

  let rootIframe = doc.querySelector("iframe");
  let subIframe = rootIframe.contentDocument.querySelector("iframe");
  let innerNode = subIframe.contentDocument.querySelector("#inner-node");

  let quad = helper.getAdjustedQuads(innerNode, "content");

  //rootIframe margin + subIframe margin + node margin + node border + node padding
  let p1x = 10 + 10 + 10 + 10 + 10;
  is(quad.p1.x, p1x, "The inner node's p1 x position is correct");

  // Same as p1x + the inner node width
  let p2x = p1x + 100;
  is(quad.p2.x, p2x, "The inner node's p2 x position is correct");
}

function takesScrollingIntoAccount(doc, helper) {
  info("Checks that the quad returned for a node inside multiple scrolled " +
    "containers takes the scroll values into account");

  // For info, the container being tested here is absolutely positioned at 0 0
  // to simplify asserting the coordinates

  info("Scroll the container nodes down");
  let scrolledNode = doc.querySelector("#scrolled-node");
  scrolledNode.scrollTop = 100;
  let subScrolledNode = doc.querySelector("#sub-scrolled-node");
  subScrolledNode.scrollTop = 200;
  let innerNode = doc.querySelector("#inner-scrolled-node");

  let quad = helper.getAdjustedQuads(innerNode, "content");
  is(quad.p1.x, 0, "p1.x of the scrolled node is correct after scrolling down");
  is(quad.p1.y, -300, "p1.y of the scrolled node is correct after scrolling down");

  info("Scrolling back up");
  scrolledNode.scrollTop = 0;
  subScrolledNode.scrollTop = 0;

  let quad = helper.getAdjustedQuads(innerNode, "content");
  is(quad.p1.x, 0, "p1.x of the scrolled node is correct after scrolling up");
  is(quad.p1.y, 0, "p1.y of the scrolled node is correct after scrolling up");
}

function takesZoomIntoAccount(doc, helper) {
  info("Checks that if the page is zoomed in/out, the quad returned is correct");

  // Hard-coding coordinates in this zoom test is a bad idea as it can vary
  // depending on the platform, so we simply test that zooming in produces a
  // bigger quad and zooming out produces a smaller quad

  let node = doc.querySelector("#simple-node-with-margin-padding-border");
  let defaultQuad = helper.getAdjustedQuads(node);

  info("Zoom in");
  window.FullZoom.enlarge();
  let zoomedInQuad = helper.getAdjustedQuads(node);

  ok(zoomedInQuad.bounds.width > defaultQuad.bounds.width,
    "The zoomed in quad is bigger than the default one");
  ok(zoomedInQuad.bounds.height > defaultQuad.bounds.height,
    "The zoomed in quad is bigger than the default one");

  info("Zoom out");
  window.FullZoom.reset();
  window.FullZoom.reduce();
  let zoomedOutQuad = helper.getAdjustedQuads(node);

  ok(zoomedOutQuad.bounds.width < defaultQuad.bounds.width,
    "The zoomed out quad is smaller than the default one");
  ok(zoomedOutQuad.bounds.height < defaultQuad.bounds.height,
    "The zoomed out quad is smaller than the default one");

  window.FullZoom.reset();
}
