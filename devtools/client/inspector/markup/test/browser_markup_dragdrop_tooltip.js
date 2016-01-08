"use strict";

// Test that tooltips don't appear when dragging over tooltip targets.

const TEST_URL = "data:text/html;charset=utf8,<img src=\"about:logo\" /><div>";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);
  let {markup} = inspector;

  info("Get the tooltip target element for the image's src attribute");
  let img = yield getContainerForSelector("img", inspector);
  let target = img.editor.getAttributeElement("src").querySelector(".link");

  info("Check that the src attribute of the image is a valid tooltip target");
  let isValid = yield markup.tooltip.isValidHoverTarget(target);
  ok(isValid, "The element is a valid tooltip target");

  info("Start dragging the test div");
  yield simulateNodeDrag(inspector, "div");

  info("Now check that the src attribute of the image isn't a valid target");
  try{
    yield markup.tooltip.isValidHoverTarget(target);
    isValid = true;
  } catch(e) {
    isValid = false;
  }
  ok(!isValid, "The element is not a valid tooltip target");

  info("Stop dragging the test div");
  yield simulateNodeDrop(inspector, "div");

  info("Check again the src attribute of the image");
  isValid = yield markup.tooltip.isValidHoverTarget(target);
  ok(isValid, "The element is a valid tooltip target");
});
