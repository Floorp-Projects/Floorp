/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test the behavior of the deduplicatePaths utility function.

function edge(from, to, name) {
  return { from, to, name };
}

function run_test() {
  const a = 1;
  const b = 2;
  const c = 3;
  const d = 4;
  const e = 5;
  const f = 6;
  const g = 7;

  dumpn("Single long path");
  assertDeduplicatedPaths({
    target: g,
    paths: [
      [
        pathEntry(a, "e1"),
        pathEntry(b, "e2"),
        pathEntry(c, "e3"),
        pathEntry(d, "e4"),
        pathEntry(e, "e5"),
        pathEntry(f, "e6"),
      ]
    ],
    expectedNodes: [a, b, c, d, e, f, g],
    expectedEdges: [
      edge(a, b, "e1"),
      edge(b, c, "e2"),
      edge(c, d, "e3"),
      edge(d, e, "e4"),
      edge(e, f, "e5"),
      edge(f, g, "e6"),
    ]
  });

  dumpn("Multiple edges from and to the same nodes");
  assertDeduplicatedPaths({
    target: a,
    paths: [
      [pathEntry(b, "x")],
      [pathEntry(b, "y")],
      [pathEntry(b, "z")],
    ],
    expectedNodes: [a, b],
    expectedEdges: [
      edge(b, a, "x"),
      edge(b, a, "y"),
      edge(b, a, "z"),
    ]
  });

  dumpn("Multiple paths sharing some nodes and edges");
  assertDeduplicatedPaths({
    target: g,
    paths: [
      [
        pathEntry(a, "a->b"),
        pathEntry(b, "b->c"),
        pathEntry(c, "foo"),
      ],
      [
        pathEntry(a, "a->b"),
        pathEntry(b, "b->d"),
        pathEntry(d, "bar"),
      ],
      [
        pathEntry(a, "a->b"),
        pathEntry(b, "b->e"),
        pathEntry(e, "baz"),
      ],
    ],
    expectedNodes: [a, b, c, d, e, g],
    expectedEdges: [
      edge(a, b, "a->b"),
      edge(b, c, "b->c"),
      edge(b, d, "b->d"),
      edge(b, e, "b->e"),
      edge(c, g, "foo"),
      edge(d, g, "bar"),
      edge(e, g, "baz"),
    ]
  });

  dumpn("Second shortest path contains target itself");
  assertDeduplicatedPaths({
    target: g,
    paths: [
      [
        pathEntry(a, "a->b"),
        pathEntry(b, "b->g"),
      ],
      [
        pathEntry(a, "a->b"),
        pathEntry(b, "b->g"),
        pathEntry(g, "g->f"),
        pathEntry(f, "f->g"),
      ],
    ],
    expectedNodes: [a, b, g],
    expectedEdges: [
      edge(a, b, "a->b"),
      edge(b, g, "b->g"),
    ]
  });
}
