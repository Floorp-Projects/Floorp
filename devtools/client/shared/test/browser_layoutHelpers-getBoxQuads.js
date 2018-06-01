/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests getAdjustedQuads works properly in a variety of use cases including
// iframes, scroll and zoom

"use strict";

const TEST_URI = TEST_URI_ROOT + "doc_layoutHelpers-getBoxQuads.html";

add_task(async function() {
  const tab = await addTab(TEST_URI);

  info("Running tests");

  // `FullZoom` isn't available from the ContentTask. This code is defined by browser
  // frontend and runs in the parent process. Here, we use the message manager
  // to allow the Content Task to call this zoom helper whenever it needs to.
  const mm = tab.linkedBrowser.messageManager;
  mm.addMessageListener("devtools-test:command", async function({ data }) {
    switch (data) {
      case "zoom-enlarge":
        window.FullZoom.enlarge();
        break;
      case "zoom-reset":
        await window.FullZoom.reset();
        break;
      case "zoom-reduce":
        window.FullZoom.reduce();
        break;
    }
    mm.sendAsyncMessage("devtools-test:done");
  });

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    // This function allows the Content Task to easily call `FullZoom` API via
    // the message manager.
    function sendCommand(cmd) {
      const onDone = new Promise(done => {
        addMessageListener("devtools-test:done", function listener() {
          removeMessageListener("devtools-test:done", listener);
          done();
        });
      });
      sendAsyncMessage("devtools-test:command", cmd);
      return onDone;
    }

    const doc = content.document;

    const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
    const {getAdjustedQuads} = require("devtools/shared/layout/utils");

    ok(typeof getAdjustedQuads === "function", "getAdjustedQuads is defined");

    returnsTheRightDataStructure();
    isEmptyForMissingNode();
    isEmptyForHiddenNodes();
    defaultsToBorderBoxIfNoneProvided();
    returnsLikeGetBoxQuadsInSimpleCase();
    takesIframesOffsetsIntoAccount();
    takesScrollingIntoAccount();
    await takesZoomIntoAccount();
    returnsMultipleItemsForWrappingInlineElements();

    function returnsTheRightDataStructure() {
      info("Checks that the returned data contains bounds and 4 points");

      const node = doc.querySelector("body");
      const [res] = getAdjustedQuads(doc.defaultView, node, "content");

      ok("bounds" in res, "The returned data has a bounds property");
      ok("p1" in res, "The returned data has a p1 property");
      ok("p2" in res, "The returned data has a p2 property");
      ok("p3" in res, "The returned data has a p3 property");
      ok("p4" in res, "The returned data has a p4 property");

      for (const boundProp of
        ["bottom", "top", "right", "left", "width", "height", "x", "y"]) {
        ok(boundProp in res.bounds, "The bounds has a " + boundProp + " property");
      }

      for (const point of ["p1", "p2", "p3", "p4"]) {
        for (const pointProp of ["x", "y", "z", "w"]) {
          ok(pointProp in res[point], point + " has a " + pointProp + " property");
        }
      }
    }

    function isEmptyForMissingNode() {
      info("Checks that null is returned for invalid nodes");

      for (const input of [null, undefined, "", 0]) {
        is(getAdjustedQuads(doc.defaultView, input).length, 0,
          "A 0-length array is returned for input " + input);
      }
    }

    function isEmptyForHiddenNodes() {
      info("Checks that null is returned for nodes that aren't rendered");

      const style = doc.querySelector("#styles");
      is(getAdjustedQuads(doc.defaultView, style).length, 0,
        "null is returned for a <style> node");

      const hidden = doc.querySelector("#hidden-node");
      is(getAdjustedQuads(doc.defaultView, hidden).length, 0,
        "null is returned for a hidden node");
    }

    function defaultsToBorderBoxIfNoneProvided() {
      info("Checks that if no boxtype is passed, then border is the default one");

      const node = doc.querySelector("#simple-node-with-margin-padding-border");
      const [withBoxType] = getAdjustedQuads(doc.defaultView, node, "border");
      const [withoutBoxType] = getAdjustedQuads(doc.defaultView, node);

      for (const boundProp of
        ["bottom", "top", "right", "left", "width", "height", "x", "y"]) {
        is(withBoxType.bounds[boundProp], withoutBoxType.bounds[boundProp],
          boundProp + " bound is equal with or without the border box type");
      }

      for (const point of ["p1", "p2", "p3", "p4"]) {
        for (const pointProp of ["x", "y", "z", "w"]) {
          is(withBoxType[point][pointProp], withoutBoxType[point][pointProp],
            point + "." + pointProp +
            " is equal with or without the border box type");
        }
      }
    }

    function returnsLikeGetBoxQuadsInSimpleCase() {
      info("Checks that for an element in the main frame, without scroll nor zoom" +
        "that the returned value is similar to the returned value of getBoxQuads");

      const node = doc.querySelector("#simple-node-with-margin-padding-border");

      for (const region of ["content", "padding", "border", "margin"]) {
        const expected = node.getBoxQuads({
          box: region
        })[0];
        const [actual] = getAdjustedQuads(doc.defaultView, node, region);

        for (const boundProp of
          ["bottom", "top", "right", "left", "width", "height", "x", "y"]) {
          is(actual.bounds[boundProp], expected.bounds[boundProp],
            boundProp + " bound is equal to the one returned by getBoxQuads for " +
            region + " box");
        }

        for (const point of ["p1", "p2", "p3", "p4"]) {
          for (const pointProp of ["x", "y", "z", "w"]) {
            is(actual[point][pointProp], expected[point][pointProp],
              point + "." + pointProp +
              " is equal to the one returned by getBoxQuads for " + region + " box");
          }
        }
      }
    }

    function takesIframesOffsetsIntoAccount() {
      info("Checks that the quad returned for a node inside iframes that have " +
        "margins takes those offsets into account");

      const rootIframe = doc.querySelector("iframe");
      const subIframe = rootIframe.contentDocument.querySelector("iframe");
      const innerNode = subIframe.contentDocument.querySelector("#inner-node");

      const [quad] = getAdjustedQuads(doc.defaultView, innerNode, "content");

      // rootIframe margin + subIframe margin + node margin + node border + node padding
      const p1x = 10 + 10 + 10 + 10 + 10;
      is(quad.p1.x, p1x, "The inner node's p1 x position is correct");

      // Same as p1x + the inner node width
      const p2x = p1x + 100;
      is(quad.p2.x, p2x, "The inner node's p2 x position is correct");
    }

    function takesScrollingIntoAccount() {
      info("Checks that the quad returned for a node inside multiple scrolled " +
        "containers takes the scroll values into account");

      // For info, the container being tested here is absolutely positioned at 0 0
      // to simplify asserting the coordinates

      info("Scroll the container nodes down");
      const scrolledNode = doc.querySelector("#scrolled-node");
      scrolledNode.scrollTop = 100;
      const subScrolledNode = doc.querySelector("#sub-scrolled-node");
      subScrolledNode.scrollTop = 200;
      const innerNode = doc.querySelector("#inner-scrolled-node");

      let [quad] = getAdjustedQuads(doc.defaultView, innerNode, "content");
      is(quad.p1.x, 0, "p1.x of the scrolled node is correct after scrolling down");
      is(quad.p1.y, -300, "p1.y of the scrolled node is correct after scrolling down");

      info("Scrolling back up");
      scrolledNode.scrollTop = 0;
      subScrolledNode.scrollTop = 0;

      [quad] = getAdjustedQuads(doc.defaultView, innerNode, "content");
      is(quad.p1.x, 0, "p1.x of the scrolled node is correct after scrolling up");
      is(quad.p1.y, 0, "p1.y of the scrolled node is correct after scrolling up");
    }

    async function takesZoomIntoAccount() {
      info("Checks that if the page is zoomed in/out, the quad returned is correct");

      // Hard-coding coordinates in this zoom test is a bad idea as it can vary
      // depending on the platform, so we simply test that zooming in produces a
      // bigger quad and zooming out produces a smaller quad

      const node = doc.querySelector("#simple-node-with-margin-padding-border");
      const [defaultQuad] = getAdjustedQuads(doc.defaultView, node);

      info("Zoom in");
      await sendCommand("zoom-enlarge");
      const [zoomedInQuad] = getAdjustedQuads(doc.defaultView, node);

      ok(zoomedInQuad.bounds.width > defaultQuad.bounds.width,
        "The zoomed in quad is bigger than the default one");
      ok(zoomedInQuad.bounds.height > defaultQuad.bounds.height,
        "The zoomed in quad is bigger than the default one");

      info("Zoom out");
      await sendCommand("zoom-reset");
      await sendCommand("zoom-reduce");

      const [zoomedOutQuad] = getAdjustedQuads(doc.defaultView, node);

      ok(zoomedOutQuad.bounds.width < defaultQuad.bounds.width,
        "The zoomed out quad is smaller than the default one");
      ok(zoomedOutQuad.bounds.height < defaultQuad.bounds.height,
        "The zoomed out quad is smaller than the default one");

      await sendCommand("zoom-reset");
    }

    function returnsMultipleItemsForWrappingInlineElements() {
      info("Checks that several quads are returned " +
           "for inline elements that span line-breaks");

      const node = doc.querySelector("#inline");
      const quads = getAdjustedQuads(doc.defaultView, node, "content");
      // At least 3 because of the 2 <br />, maybe more depending on the window size.
      ok(quads.length >= 3, "Multiple quads were returned");

      is(quads.length, node.getBoxQuads().length,
        "The same number of boxes as getBoxQuads was returned");
    }
  });

  gBrowser.removeCurrentTab();
});
