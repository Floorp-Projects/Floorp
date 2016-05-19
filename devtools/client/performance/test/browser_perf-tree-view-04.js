/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler's tree view implementation works properly and
 * creates the correct DOM nodes in the correct order.
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

  is(treeRoot.target.getAttribute("origin"), "chrome",
    "The root node's 'origin' attribute is correct.");
  is(treeRoot.target.getAttribute("category"), "",
    "The root node's 'category' attribute is correct.");
  is(treeRoot.target.getAttribute("tooltiptext"), "",
    "The root node's 'tooltiptext' attribute is correct.");
  is(treeRoot.target.querySelector(".call-tree-category"), null,
    "The root node's category label cell should be hidden.");

  let A = treeRoot.getChild();
  let B = A.getChild();
  let D = B.getChild();

  is(D.target.getAttribute("origin"), "chrome",
    "The .A.B.D node's 'origin' attribute is correct.");
  is(D.target.getAttribute("category"), "gc",
    "The .A.B.D node's 'category' attribute is correct.");
  is(D.target.getAttribute("tooltiptext"), "D (http://foo/bar/baz:78:9)",
    "The .A.B.D node's 'tooltiptext' attribute is correct.");

  is(D.target.childNodes.length, 6,
    "The number of columns displayed for tree items is correct.");
  is(D.target.childNodes[0].getAttribute("type"), "duration",
    "The first column displayed for tree items is correct.");
  is(D.target.childNodes[1].getAttribute("type"), "percentage",
    "The third column displayed for tree items is correct.");
  is(D.target.childNodes[2].getAttribute("type"), "self-duration",
    "The second column displayed for tree items is correct.");
  is(D.target.childNodes[3].getAttribute("type"), "self-percentage",
    "The fourth column displayed for tree items is correct.");
  is(D.target.childNodes[4].getAttribute("type"), "samples",
    "The fifth column displayed for tree items is correct.");
  is(D.target.childNodes[5].getAttribute("type"), "function",
    "The sixth column displayed for tree items is correct.");

  let functionCell = D.target.childNodes[5];

  is(functionCell.childNodes.length, 7,
    "The number of columns displayed for function cells is correct.");
  is(functionCell.childNodes[0].className, "arrow theme-twisty",
    "The first node displayed for function cells is correct.");
  is(functionCell.childNodes[1].className, "plain call-tree-name",
    "The second node displayed for function cells is correct.");
  is(functionCell.childNodes[2].className, "plain call-tree-url",
    "The third node displayed for function cells is correct.");
  is(functionCell.childNodes[3].className, "plain call-tree-line",
    "The fourth node displayed for function cells is correct.");
  is(functionCell.childNodes[4].className, "plain call-tree-column",
    "The fifth node displayed for function cells is correct.");
  is(functionCell.childNodes[5].className, "plain call-tree-host",
    "The sixth node displayed for function cells is correct.");
  is(functionCell.childNodes[6].className, "plain call-tree-category",
    "The seventh node displayed for function cells is correct.");
});
