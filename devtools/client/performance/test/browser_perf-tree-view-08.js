/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the profiler's tree view renders generalized platform data
 * when `contentOnly` is on correctly.
 */

const {
  ThreadNode,
} = require("devtools/client/performance/modules/logic/tree-model");
const {
  CallView,
} = require("devtools/client/performance/modules/widgets/tree-view");
const {
  CATEGORY_INDEX,
} = require("devtools/client/performance/modules/categories");
const RecordingUtils = require("devtools/shared/performance/recording-utils");

add_task(function() {
  const threadNode = new ThreadNode(gProfile.threads[0], {
    startTime: 0,
    endTime: 20,
    contentOnly: true,
  });

  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;

  const treeRoot = new CallView({ frame: threadNode, autoExpandDepth: 10 });
  const container = document.createXULElement("vbox");
  treeRoot.attachTo(container);

  /*
   * (root)
   *   - A
   *     - B
   *       - C
   *       - D
   *     - (GC)
   *     - E
   *       - F
   *         - (JS)
   *   - (JS)
   */

  const A = treeRoot.getChild(0);
  const JS = treeRoot.getChild(1);
  const GC = A.getChild(1);
  const JS2 = A.getChild(2)
    .getChild()
    .getChild();

  is(
    JS.target.getAttribute("category"),
    "js",
    "Generalized JS node has correct category"
  );
  is(
    JS.target.getAttribute("tooltiptext"),
    "JIT",
    "Generalized JS node has correct category"
  );
  is(
    JS.target.querySelector(".call-tree-name").textContent.trim(),
    "JIT",
    "Generalized JS node has correct display value as just the category name."
  );

  is(
    JS2.target.getAttribute("category"),
    "js",
    "Generalized second JS node has correct category"
  );
  is(
    JS2.target.getAttribute("tooltiptext"),
    "JIT",
    "Generalized second JS node has correct category"
  );
  is(
    JS2.target.querySelector(".call-tree-name").textContent.trim(),
    "JIT",
    "Generalized second JS node has correct display value as just the category name."
  );

  is(
    GC.target.getAttribute("category"),
    "gc",
    "Generalized GC node has correct category"
  );
  is(
    GC.target.getAttribute("tooltiptext"),
    "GC",
    "Generalized GC node has correct category"
  );
  is(
    GC.target.querySelector(".call-tree-name").textContent.trim(),
    "GC",
    "Generalized GC node has correct display value as just the category name."
  );
});

const gProfile = RecordingUtils.deflateProfile({
  meta: { version: 2 },
  threads: [
    {
      samples: [
        {
          time: 1,
          frames: [
            { location: "(root)" },
            { location: "http://content/A" },
            { location: "http://content/B" },
            { location: "http://content/C" },
          ],
        },
        {
          time: 1 + 1,
          frames: [
            { location: "(root)" },
            { location: "http://content/A" },
            { location: "http://content/B" },
            { location: "http://content/D" },
          ],
        },
        {
          time: 1 + 1 + 2,
          frames: [
            { location: "(root)" },
            { location: "http://content/A" },
            { location: "http://content/E" },
            { location: "http://content/F" },
            { location: "platform_JS", category: CATEGORY_INDEX("js") },
          ],
        },
        {
          time: 1 + 1 + 2 + 3,
          frames: [
            { location: "(root)" },
            { location: "platform_JS2", category: CATEGORY_INDEX("js") },
          ],
        },
        {
          time: 1 + 1 + 2 + 3 + 5,
          frames: [
            { location: "(root)" },
            { location: "http://content/A" },
            { location: "platform_GC", category: CATEGORY_INDEX("gc") },
          ],
        },
      ],
    },
  ],
});
