/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Verifies if FrameNodes retain and parse their data appropriately.
 */

function run_test() {
  run_next_test();
}

add_task(function test() {
  let FrameUtils = require("devtools/client/performance/modules/logic/frame-utils");
  let { FrameNode } = require("devtools/client/performance/modules/logic/tree-model");
  let { CATEGORY_OTHER } = require("devtools/client/performance/modules/global");
  let compute = frame => {
    FrameUtils.computeIsContentAndCategory(frame);
    return frame;
  };

  let frames = [
    new FrameNode("hello/<.world (http://foo/bar.js:123:987)", compute({
      location: "hello/<.world (http://foo/bar.js:123:987)",
      line: 456,
    }), false),
    new FrameNode("hello/<.world (http://foo/bar.js#baz:123:987)", compute({
      location: "hello/<.world (http://foo/bar.js#baz:123:987)",
      line: 456,
    }), false),
    new FrameNode("hello/<.world (http://foo/#bar:123:987)", compute({
      location: "hello/<.world (http://foo/#bar:123:987)",
      line: 456,
    }), false),
    new FrameNode("hello/<.world (http://foo/:123:987)", compute({
      location: "hello/<.world (http://foo/:123:987)",
      line: 456,
    }), false),
    new FrameNode("hello/<.world (resource://foo.js -> http://bar/baz.js:123:987)", compute({
      location: "hello/<.world (resource://foo.js -> http://bar/baz.js:123:987)",
      line: 456,
    }), false),
    new FrameNode("Foo::Bar::Baz", compute({
      location: "Foo::Bar::Baz",
      line: 456,
      category: CATEGORY_OTHER,
    }), false),
    new FrameNode("EnterJIT", compute({
      location: "EnterJIT",
    }), false),
    new FrameNode("chrome://browser/content/content.js", compute({
      location: "chrome://browser/content/content.js",
      line: 456,
      column: 123
    }), false),
    new FrameNode("hello/<.world (resource://gre/foo.js:123:434)", compute({
      location: "hello/<.world (resource://gre/foo.js:123:434)",
      line: 456
    }), false),
    new FrameNode("main (http://localhost:8888/file.js:123:987)", compute({
      location: "main (http://localhost:8888/file.js:123:987)",
      line: 123,
    }), false),
    new FrameNode("main (resource://gre/modules/devtools/timeline.js:123)", compute({
      location: "main (resource://gre/modules/devtools/timeline.js:123)",
    }), false),
  ];

  let fields = ["nodeType", "functionName", "fileName", "hostName", "url", "line", "column", "categoryData.abbrev", "isContent", "port"]
  let expected = [
    // nodeType, functionName, fileName, hostName, url, line, column, categoryData.abbrev, isContent, port
    ["Frame", "hello/<.world", "bar.js", "foo", "http://foo/bar.js", 123, 987, void 0, true],
    ["Frame", "hello/<.world", "bar.js", "foo", "http://foo/bar.js#baz", 123, 987, void 0, true],
    ["Frame", "hello/<.world", "/", "foo", "http://foo/#bar", 123, 987, void 0, true],
    ["Frame", "hello/<.world", "/", "foo", "http://foo/", 123, 987, void 0, true],
    ["Frame", "hello/<.world", "baz.js", "bar", "http://bar/baz.js", 123, 987, "other", false],
    ["Frame", "Foo::Bar::Baz", null, null, null, 456, void 0, "other", false],
    ["Frame", "EnterJIT", null, null, null, null, null, "js", false],
    ["Frame", "chrome://browser/content/content.js", null, null, null, 456, null, "other", false],
    ["Frame", "hello/<.world", "foo.js", null, "resource://gre/foo.js", 123, 434, "other", false],
    ["Frame", "main", "file.js", "localhost", "http://localhost:8888/file.js", 123, 987, null, true, 8888],
    ["Frame", "main", "timeline.js", null, "resource://gre/modules/devtools/timeline.js", 123, null, "tools", false]
  ];

  for (let i = 0; i < frames.length; i++) {
    let info = frames[i].getInfo();
    let expect = expected[i];

    for (let j = 0; j < fields.length; j++) {
      let field = fields[j];
      let value = field === "categoryData.abbrev" ? info.categoryData.abbrev : info[field];
      equal(value, expect[j], `${field} for frame #${i} is correct: ${expect[j]}`);
    }
  }
});
