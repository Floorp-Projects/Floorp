/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let test = asyncTest(function*() {
  let style = "div { position: absolute; top: 50px; left: 50px; height: 10px; " +
              "width: 10px; border: 10px solid black; padding: 10px; margin: 10px;}";
  let html = "<style>" + style + "</style><div></div>";

  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(html));

  let {inspector, view} = yield openLayoutView();
  yield selectNode("div", inspector);
  yield runTests(inspector, view);
});

addTest("Test that the initial values of the box model are correct",
function*(inspector, view) {
  yield testGuideOnLayoutHover("margins", {
    top: { x1: "0", x2: "100%", y1: "119.5", y2: "119.5" },
    right: { x1: "50.5", x2: "50.5", y1: "0", y2: "100%" },
    bottom: { x1: "0", x2: "100%", y1: "50.5", y2: "50.5" },
    left: { x1: "119.5", x2: "119.5", y1: "0", y2: "100%" }
  }, inspector, view);

  yield testGuideOnLayoutHover("borders", {
    top: { x1: "0", x2: "100%", y1: "109.5", y2: "109.5" },
    right: { x1: "60.5", x2: "60.5", y1: "0", y2: "100%" },
    bottom: { x1: "0", x2: "100%", y1: "60.5", y2: "60.5" },
    left: { x1: "109.5", x2: "109.5", y1: "0", y2: "100%" }
  }, inspector, view);

  yield testGuideOnLayoutHover("padding", {
    top: { x1: "0", x2: "100%", y1: "99.5", y2: "99.5" },
    right: { x1: "70.5", x2: "70.5", y1: "0", y2: "100%" },
    bottom: { x1: "0", x2: "100%", y1: "70.5", y2: "70.5" },
    left: { x1: "99.5", x2: "99.5", y1: "0", y2: "100%" }
  }, inspector, view);

  yield testGuideOnLayoutHover("content", {
    top: { x1: "0", x2: "100%", y1: "79.5", y2: "79.5" },
    right: { x1: "90.5", x2: "90.5", y1: "0", y2: "100%" },
    bottom: { x1: "0", x2: "100%", y1: "90.5", y2: "90.5" },
    left: { x1: "79.5", x2: "79.5", y1: "0", y2: "100%" }
  }, inspector, view);

  gBrowser.removeCurrentTab();
});

function* testGuideOnLayoutHover(id, expected, inspector, view) {
  info("Checking " + id);

  let elt = view.doc.getElementById(id);

  EventUtils.synthesizeMouse(elt, 2, 2, {type:'mouseover'}, view.doc.defaultView);

  yield inspector.toolbox.once("highlighter-ready");

  let guideTop = getGuideStatus("top");
  let guideRight = getGuideStatus("right");
  let guideBottom = getGuideStatus("bottom");
  let guideLeft = getGuideStatus("left");

  is(guideTop.x1, expected.top.x1, "top x1 is correct");
  is(guideTop.x2, expected.top.x2, "top x2 is correct");
  is(guideTop.y1, expected.top.y1, "top y1 is correct");
  is(guideTop.y2, expected.top.y2, "top y2 is correct");

  is(guideRight.x1, expected.right.x1, "right x1 is correct");
  is(guideRight.x2, expected.right.x2, "right x2 is correct");
  is(guideRight.y1, expected.right.y1, "right y1 is correct");
  is(guideRight.y2, expected.right.y2, "right y2 is correct");

  is(guideBottom.x1, expected.bottom.x1, "bottom x1 is correct");
  is(guideBottom.x2, expected.bottom.x2, "bottom x2 is correct");
  is(guideBottom.y1, expected.bottom.y1, "bottom y1 is correct");
  is(guideBottom.y2, expected.bottom.y2, "bottom y2 is correct");

  is(guideLeft.x1, expected.left.x1, "left x1 is correct");
  is(guideLeft.x2, expected.left.x2, "left x2 is correct");
  is(guideLeft.y1, expected.left.y1, "left y1 is correct");
  is(guideLeft.y2, expected.left.y2, "left y2 is correct");
}
