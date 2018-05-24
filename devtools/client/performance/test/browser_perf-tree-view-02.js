/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler's tree view implementation works properly and
 * creates the correct column structure after expanding some of the nodes.
 * Also tests that demangling works.
 */

const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
const { CallView } = require("devtools/client/performance/modules/widgets/tree-view");
const { synthesizeProfile } = require("devtools/client/performance/test/helpers/synth-utils");

const MANGLED_FN = "__Z3FooIiEvv";
const UNMANGLED_FN = "void Foo<int>()";

add_task(function() {
  // Create a profile and mangle a function inside the string table.
  const profile = synthesizeProfile();

  profile.threads[0].stringTable[1] =
    profile.threads[0].stringTable[1].replace("A (", `${MANGLED_FN} (`);

  const threadNode = new ThreadNode(profile.threads[0], { startTime: 0, endTime: 20 });

  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;

  const treeRoot = new CallView({ frame: threadNode });
  const container = document.createElement("vbox");
  treeRoot.autoExpandDepth = 0;
  treeRoot.attachTo(container);

  const $$ = node => container.querySelectorAll(node);
  const $fun = (node, ancestor) => (ancestor || container).querySelector(
    ".call-tree-cell[type=function] > " + node);
  const $$fun = (node, ancestor) => (ancestor || container).querySelectorAll(
    ".call-tree-cell[type=function] > " + node);
  const $$dur = i => container.querySelectorAll(".call-tree-cell[type=duration]")[i];
  const $$per = i => container.querySelectorAll(".call-tree-cell[type=percentage]")[i];
  const $$sam = i => container.querySelectorAll(".call-tree-cell[type=samples]")[i];

  is(container.childNodes.length, 1,
    "The container node should have one child available.");
  is(container.childNodes[0].className, "call-tree-item",
    "The root node in the tree has the correct class name.");

  is($$dur(0).textContent.trim(), "20 ms",
    "The root's duration cell displays the correct value.");
  is($$per(0).textContent.trim(), "100%",
    "The root's percentage cell displays the correct value.");
  is($$sam(0).textContent.trim(), "0",
    "The root's samples cell displays the correct value.");
  is($$fun(".call-tree-name")[0].textContent.trim(), "(root)",
    "The root's function cell displays the correct name.");
  is($$fun(".call-tree-url")[0], null,
    "The root's function cell displays no url.");
  is($$fun(".call-tree-line")[0], null,
    "The root's function cell displays no line.");
  is($$fun(".call-tree-host")[0], null,
    "The root's function cell displays no host.");
  is($$fun(".call-tree-category")[0], null,
    "The root's function cell displays no category.");

  treeRoot.expand();

  is(container.childNodes.length, 2,
    "The container node should have two children available.");
  is(container.childNodes[0].className, "call-tree-item",
    "The root node in the tree has the correct class name.");
  is(container.childNodes[1].className, "call-tree-item",
    "The .A node in the tree has the correct class name.");

  // Test demangling in the profiler tree.
  is($$dur(1).textContent.trim(), "20 ms",
    "The .A node's duration cell displays the correct value.");
  is($$per(1).textContent.trim(), "100%",
    "The .A node's percentage cell displays the correct value.");
  is($$sam(1).textContent.trim(), "0",
    "The .A node's samples cell displays the correct value.");

  is($fun(".call-tree-name", $$(".call-tree-item")[1]).textContent.trim(), UNMANGLED_FN,
    "The .A node's function cell displays the correct name.");
  is($fun(".call-tree-url", $$(".call-tree-item")[1]).textContent.trim(), "baz",
    "The .A node's function cell displays the correct url.");
  is($fun(".call-tree-line", $$(".call-tree-item")[1]).textContent.trim(), ":12",
    "The .A node's function cell displays the correct line.");
  is($fun(".call-tree-host", $$(".call-tree-item")[1]).textContent.trim(), "foo",
    "The .A node's function cell displays the correct host.");
  is($fun(".call-tree-category", $$(".call-tree-item")[1]).textContent.trim(), "Gecko",
    "The .A node's function cell displays the correct category.");

  ok($$(".call-tree-item")[1].getAttribute("tooltiptext").includes(MANGLED_FN),
    "The .A node's row's tooltip contains the original mangled name.");

  const A = treeRoot.getChild();
  A.expand();

  is(container.childNodes.length, 4,
    "The container node should have four children available.");
  is(container.childNodes[0].className, "call-tree-item",
    "The root node in the tree has the correct class name.");
  is(container.childNodes[1].className, "call-tree-item",
    "The .A node in the tree has the correct class name.");
  is(container.childNodes[2].className, "call-tree-item",
    "The .B node in the tree has the correct class name.");
  is(container.childNodes[3].className, "call-tree-item",
    "The .E node in the tree has the correct class name.");

  is($$dur(2).textContent.trim(), "15 ms",
    "The .A.B node's duration cell displays the correct value.");
  is($$per(2).textContent.trim(), "75%",
    "The .A.B node's percentage cell displays the correct value.");
  is($$sam(2).textContent.trim(), "0",
    "The .A.B node's samples cell displays the correct value.");
  is($fun(".call-tree-name", $$(".call-tree-item")[2]).textContent.trim(), "B",
    "The .A.B node's function cell displays the correct name.");
  is($fun(".call-tree-url", $$(".call-tree-item")[2]).textContent.trim(), "baz",
    "The .A.B node's function cell displays the correct url.");
  ok($fun(".call-tree-url", $$(".call-tree-item")[2]).getAttribute("tooltiptext").includes("http://foo/bar/baz"),
    "The .A.B node's function cell displays the correct url tooltiptext.");
  is($fun(".call-tree-line", $$(".call-tree-item")[2]).textContent.trim(), ":34",
    "The .A.B node's function cell displays the correct line.");
  is($fun(".call-tree-host", $$(".call-tree-item")[2]).textContent.trim(), "foo",
    "The .A.B node's function cell displays the correct host.");
  is($fun(".call-tree-category", $$(".call-tree-item")[2]).textContent.trim(), "Layout",
    "The .A.B node's function cell displays the correct category.");

  is($$dur(3).textContent.trim(), "5 ms",
    "The .A.E node's duration cell displays the correct value.");
  is($$per(3).textContent.trim(), "25%",
    "The .A.E node's percentage cell displays the correct value.");
  is($$sam(3).textContent.trim(), "0",
    "The .A.E node's samples cell displays the correct value.");
  is($fun(".call-tree-name", $$(".call-tree-item")[3]).textContent.trim(), "E",
    "The .A.E node's function cell displays the correct name.");
  is($fun(".call-tree-url", $$(".call-tree-item")[3]).textContent.trim(), "baz",
    "The .A.E node's function cell displays the correct url.");
  ok($fun(".call-tree-url", $$(".call-tree-item")[3]).getAttribute("tooltiptext").includes("http://foo/bar/baz"),
    "The .A.E node's function cell displays the correct url tooltiptext.");
  is($fun(".call-tree-line", $$(".call-tree-item")[3]).textContent.trim(), ":90",
    "The .A.E node's function cell displays the correct line.");
  is($fun(".call-tree-host", $$(".call-tree-item")[3]).textContent.trim(), "foo",
    "The .A.E node's function cell displays the correct host.");
  is($fun(".call-tree-category", $$(".call-tree-item")[3]).textContent.trim(), "GC",
    "The .A.E node's function cell displays the correct category.");
});
