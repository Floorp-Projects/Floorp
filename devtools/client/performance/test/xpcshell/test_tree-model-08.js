/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Verifies if FrameNodes retain and parse their data appropriately.
 */

add_task(function test() {
  const FrameUtils = require("devtools/client/performance/modules/logic/frame-utils");
  const {
    FrameNode,
  } = require("devtools/client/performance/modules/logic/tree-model");
  const {
    CATEGORY_INDEX,
  } = require("devtools/client/performance/modules/categories");
  const compute = frame => {
    FrameUtils.computeIsContentAndCategory(frame);
    return frame;
  };

  const frames = [
    new FrameNode(
      "hello/<.world (http://foo/bar.js:123:987)",
      compute({
        location: "hello/<.world (http://foo/bar.js:123:987)",
        line: 456,
      }),
      false
    ),
    new FrameNode(
      "hello/<.world (http://foo/bar.js#baz:123:987)",
      compute({
        location: "hello/<.world (http://foo/bar.js#baz:123:987)",
        line: 456,
      }),
      false
    ),
    new FrameNode(
      "hello/<.world (http://foo/#bar:123:987)",
      compute({
        location: "hello/<.world (http://foo/#bar:123:987)",
        line: 456,
      }),
      false
    ),
    new FrameNode(
      "hello/<.world (http://foo/:123:987)",
      compute({
        location: "hello/<.world (http://foo/:123:987)",
        line: 456,
      }),
      false
    ),
    new FrameNode(
      "hello/<.world (resource://foo.js -> http://bar/baz.js:123:987)",
      compute({
        location:
          "hello/<.world (resource://foo.js -> http://bar/baz.js:123:987)",
        line: 456,
      }),
      false
    ),
    new FrameNode(
      "Foo::Bar::Baz",
      compute({
        location: "Foo::Bar::Baz",
        line: 456,
        category: CATEGORY_INDEX("other"),
      }),
      false
    ),
    new FrameNode(
      "EnterJIT",
      compute({
        location: "EnterJIT",
      }),
      false
    ),
    new FrameNode(
      "chrome://browser/content/content.js",
      compute({
        location: "chrome://browser/content/content.js",
        line: 456,
        column: 123,
      }),
      false
    ),
    new FrameNode(
      "hello/<.world (resource://gre/foo.js:123:434)",
      compute({
        location: "hello/<.world (resource://gre/foo.js:123:434)",
        line: 456,
      }),
      false
    ),
    new FrameNode(
      "main (http://localhost:8888/file.js:123:987)",
      compute({
        location: "main (http://localhost:8888/file.js:123:987)",
        line: 123,
      }),
      false
    ),
    new FrameNode(
      "main (resource://devtools/timeline.js:123)",
      compute({
        location: "main (resource://devtools/timeline.js:123)",
      }),
      false
    ),
  ];

  const fields = [
    "nodeType",
    "functionName",
    "fileName",
    "host",
    "url",
    "line",
    "column",
    "categoryData.abbrev",
    "isContent",
    "port",
  ];
  const expected = [
    // nodeType, functionName, fileName, host, url, line, column, categoryData.abbrev,
    // isContent, port
    [
      "Frame",
      "hello/<.world",
      "bar.js",
      "foo",
      "http://foo/bar.js",
      123,
      987,
      void 0,
      true,
    ],
    [
      "Frame",
      "hello/<.world",
      "bar.js",
      "foo",
      "http://foo/bar.js#baz",
      123,
      987,
      void 0,
      true,
    ],
    [
      "Frame",
      "hello/<.world",
      "/",
      "foo",
      "http://foo/#bar",
      123,
      987,
      void 0,
      true,
    ],
    [
      "Frame",
      "hello/<.world",
      "/",
      "foo",
      "http://foo/",
      123,
      987,
      void 0,
      true,
    ],
    [
      "Frame",
      "hello/<.world",
      "baz.js",
      "bar",
      "http://bar/baz.js",
      123,
      987,
      "other",
      false,
    ],
    ["Frame", "Foo::Bar::Baz", null, null, null, 456, void 0, "other", false],
    ["Frame", "EnterJIT", null, null, null, null, null, "js", false],
    [
      "Frame",
      "chrome://browser/content/content.js",
      null,
      null,
      null,
      456,
      null,
      "other",
      false,
    ],
    [
      "Frame",
      "hello/<.world",
      "foo.js",
      null,
      "resource://gre/foo.js",
      123,
      434,
      "other",
      false,
    ],
    [
      "Frame",
      "main",
      "file.js",
      "localhost:8888",
      "http://localhost:8888/file.js",
      123,
      987,
      null,
      true,
      8888,
    ],
    [
      "Frame",
      "main",
      "timeline.js",
      null,
      "resource://devtools/timeline.js",
      123,
      null,
      "tools",
      false,
    ],
  ];

  for (let i = 0; i < frames.length; i++) {
    const info = frames[i].getInfo();
    const expect = expected[i];

    for (let j = 0; j < fields.length; j++) {
      const field = fields[j];
      const value =
        field === "categoryData.abbrev"
          ? info.categoryData.abbrev
          : info[field];
      equal(
        value,
        expect[j],
        `${field} for frame #${i} is correct: ${expect[j]}`
      );
    }
  }
});
