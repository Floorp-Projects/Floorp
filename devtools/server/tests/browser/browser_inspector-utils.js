/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/inspector-helpers.js",
  this
);

const COLOR_WHITE = [255, 255, 255, 1];

add_task(async function loadNewChild() {
  const { walker } = await initInspectorFront(
    `data:text/html,<style>body{color:red;background-color:white;}body::before{content:"test";}</style>`
  );

  const body = await walker.querySelector(walker.rootNode, "body");
  const color = await body.getBackgroundColor();
  Assert.deepEqual(
    color.value,
    COLOR_WHITE,
    "Background color is calculated correctly for an element with a pseudo child."
  );
});
