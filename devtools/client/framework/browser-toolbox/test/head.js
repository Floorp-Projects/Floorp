/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../shared/test/shared-head.js */
/* import-globals-from ../../../shared/test/telemetry-test-helpers.js */

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

/* import-globals-from helpers-browser-toolbox.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);

async function selectNodeFront(inspector, walker, selector, rootNode) {
  rootNode = rootNode || walker.rootNode;
  const nodeFront = await walker.querySelector(rootNode, selector);
  const updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront);
  await updated;
  return nodeFront;
}
