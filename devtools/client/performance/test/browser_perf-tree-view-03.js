/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler's tree view implementation works properly and
 * creates the correct column structure and can auto-expand all nodes.
 */

const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
const { CallView } = require("devtools/client/performance/modules/widgets/tree-view");
const { synthesizeProfile } = require("devtools/client/performance/test/helpers/synth-utils");

add_task(function () {
  let profile = synthesizeProfile();
  let threadNode = new ThreadNode(profile.threads[0], { startTime: 0, endTime: 20 });

  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;

  let treeRoot = new CallView({ frame: threadNode });
  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  let $$fun = i => container.querySelectorAll(".call-tree-cell[type=function]")[i];
  let $$nam = i => container.querySelectorAll(
    ".call-tree-cell[type=function] > .call-tree-name")[i];
  let $$dur = i => container.querySelectorAll(".call-tree-cell[type=duration]")[i];

  is(container.childNodes.length, 7,
    "The container node should have all children available.");
  is(Array.filter(container.childNodes, e => e.className != "call-tree-item").length, 0,
    "All item nodes in the tree have the correct class name.");

  is($$fun(0).style.marginInlineStart, "0px",
    "The root node's function cell has the correct indentation.");
  is($$fun(1).style.marginInlineStart, "16px",
    "The .A node's function cell has the correct indentation.");
  is($$fun(2).style.marginInlineStart, "32px",
    "The .A.B node's function cell has the correct indentation.");
  is($$fun(3).style.marginInlineStart, "48px",
    "The .A.B.D node's function cell has the correct indentation.");
  is($$fun(4).style.marginInlineStart, "48px",
    "The .A.B.C node's function cell has the correct indentation.");
  is($$fun(5).style.marginInlineStart, "32px",
    "The .A.E node's function cell has the correct indentation.");
  is($$fun(6).style.marginInlineStart, "48px",
    "The .A.E.F node's function cell has the correct indentation.");

  is($$nam(0).textContent.trim(), "(root)",
    "The root node's function cell displays the correct name.");
  is($$nam(1).textContent.trim(), "A",
    "The .A node's function cell displays the correct name.");
  is($$nam(2).textContent.trim(), "B",
    "The .A.B node's function cell displays the correct name.");
  is($$nam(3).textContent.trim(), "D",
    "The .A.B.D node's function cell displays the correct name.");
  is($$nam(4).textContent.trim(), "C",
    "The .A.B.C node's function cell displays the correct name.");
  is($$nam(5).textContent.trim(), "E",
    "The .A.E node's function cell displays the correct name.");
  is($$nam(6).textContent.trim(), "F",
    "The .A.E.F node's function cell displays the correct name.");

  is($$dur(0).textContent.trim(), "20 ms",
    "The root node's function cell displays the correct duration.");
  is($$dur(1).textContent.trim(), "20 ms",
    "The .A node's function cell displays the correct duration.");
  is($$dur(2).textContent.trim(), "15 ms",
    "The .A.B node's function cell displays the correct duration.");
  is($$dur(3).textContent.trim(), "10 ms",
    "The .A.B.D node's function cell displays the correct duration.");
  is($$dur(4).textContent.trim(), "5 ms",
    "The .A.B.C node's function cell displays the correct duration.");
  is($$dur(5).textContent.trim(), "5 ms",
    "The .A.E node's function cell displays the correct duration.");
  is($$dur(6).textContent.trim(), "5 ms",
    "The .A.E.F node's function cell displays the correct duration.");
});
