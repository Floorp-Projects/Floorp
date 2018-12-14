/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Check getSources functionality with sourcemaps.
 */

const {SourceNode} = require("source-map");

add_task(threadClientTest(async ({ threadClient, debuggee }) => {
  await threadClient.reconfigure({ useSourceMaps: true });
  addSources(debuggee);

  const res = await threadClient.getSources();
  Assert.equal(res.sources.length, 3, "3 sources exist");

  await threadClient.reconfigure({ useSourceMaps: false });

  const res2 = await threadClient.getSources();
  Assert.equal(res2.sources.length, 1, "1 source exist");
}, {
  // Bug 1304144 - This test does not run in a worker because the
  // `rpc` method which talks to the main thread does not work.
  doNotRunWorker: true,
}));

function addSources(debuggee) {
  let { code, map } = (new SourceNode(null, null, null, [
    new SourceNode(1, 0, "a.js", "function a() { return 'a'; }\n"),
    new SourceNode(1, 0, "b.js", "function b() { return 'b'; }\n"),
    new SourceNode(1, 0, "c.js", "function c() { return 'c'; }\n"),
  ])).toStringWithSourceMap({
    file: "abc.js",
    sourceRoot: "http://example.com/www/js/",
  });

  code += "//# sourceMappingURL=data:text/json;base64," + btoa(map.toString());

  Cu.evalInSandbox(code, debuggee, "1.8",
                   "http://example.com/www/js/abc.js", 1);
}
