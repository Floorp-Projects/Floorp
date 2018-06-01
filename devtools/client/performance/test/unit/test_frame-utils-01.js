/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that frame-utils isContent and parseLocation work as intended
 * when parsing over frames from the profiler.
 */

const CONTENT_LOCATIONS = [
  "hello/<.world (https://foo/bar.js:123:987)",
  "hello/<.world (http://foo/bar.js:123:987)",
  "hello/<.world (http://foo/bar.js:123)",
  "hello/<.world (http://foo/bar.js#baz:123:987)",
  "hello/<.world (http://foo/bar.js?myquery=params&search=1:123:987)",
  "hello/<.world (http://foo/#bar:123:987)",
  "hello/<.world (http://foo/:123:987)",

  // Test scripts with port numbers (bug 1164131)
  "hello/<.world (http://localhost:8888/file.js:100:1)",
  "hello/<.world (http://localhost:8888/file.js:100)",

  // Eval
  "hello/<.world (http://localhost:8888/file.js line 65 > eval:1)",

  // Occurs when executing an inline script on a root html page with port
  // (I've never seen it with a column number but check anyway) bug 1164131
  "hello/<.world (http://localhost:8888/:1)",
  "hello/<.world (http://localhost:8888/:100:50)",

  // bug 1197636
  "Native[\"arraycopy(blah)\"] (http://localhost:8888/profiler.html:4)",
  "Native[\"arraycopy(blah)\"] (http://localhost:8888/profiler.html:4:5)",
].map(argify);

const CHROME_LOCATIONS = [
  { location: "Startup::XRE_InitChildProcess", line: 456, column: 123 },
  { location: "chrome://browser/content/content.js", line: 456, column: 123 },
  "setTimeout_timer (resource://gre/foo.js:123:434)",
  "hello/<.world (jar:file://Users/mcurie/Dev/jetpacks.js)",
  "hello/<.world (resource://foo.js -> http://bar/baz.js:123:987)",
  "EnterJIT",
].map(argify);

add_task(function() {
  const { computeIsContentAndCategory, parseLocation } = require("devtools/client/performance/modules/logic/frame-utils");
  const isContent = (frame) => {
    computeIsContentAndCategory(frame);
    return frame.isContent;
  };

  for (const frame of CONTENT_LOCATIONS) {
    ok(isContent.apply(null, frameify(frame)),
       `${frame[0]} should be considered a content frame.`);
  }

  for (const frame of CHROME_LOCATIONS) {
    ok(!isContent.apply(null, frameify(frame)),
       `${frame[0]} should not be considered a content frame.`);
  }

  // functionName, fileName, host, url, line, column
  const FIELDS = ["functionName", "fileName", "host", "url", "line", "column", "host",
                  "port"];

  /* eslint-disable max-len */
  const PARSED_CONTENT = [
    ["hello/<.world", "bar.js", "foo", "https://foo/bar.js", 123, 987, "foo", null],
    ["hello/<.world", "bar.js", "foo", "http://foo/bar.js", 123, 987, "foo", null],
    ["hello/<.world", "bar.js", "foo", "http://foo/bar.js", 123, null, "foo", null],
    ["hello/<.world", "bar.js", "foo", "http://foo/bar.js#baz", 123, 987, "foo", null],
    ["hello/<.world", "bar.js", "foo", "http://foo/bar.js?myquery=params&search=1", 123, 987, "foo", null],
    ["hello/<.world", "/", "foo", "http://foo/#bar", 123, 987, "foo", null],
    ["hello/<.world", "/", "foo", "http://foo/", 123, 987, "foo", null],
    ["hello/<.world", "file.js", "localhost:8888", "http://localhost:8888/file.js", 100, 1, "localhost:8888", 8888],
    ["hello/<.world", "file.js", "localhost:8888", "http://localhost:8888/file.js", 100, null, "localhost:8888", 8888],
    ["hello/<.world", "file.js (eval:1)", "localhost:8888", "http://localhost:8888/file.js", 65, null, "localhost:8888", 8888],
    ["hello/<.world", "/", "localhost:8888", "http://localhost:8888/", 1, null, "localhost:8888", 8888],
    ["hello/<.world", "/", "localhost:8888", "http://localhost:8888/", 100, 50, "localhost:8888", 8888],
    ["Native[\"arraycopy(blah)\"]", "profiler.html", "localhost:8888", "http://localhost:8888/profiler.html", 4, null, "localhost:8888", 8888],
    ["Native[\"arraycopy(blah)\"]", "profiler.html", "localhost:8888", "http://localhost:8888/profiler.html", 4, 5, "localhost:8888", 8888],
  ];
  /* eslint-enable max-len */

  for (let i = 0; i < PARSED_CONTENT.length; i++) {
    const parsed = parseLocation.apply(null, CONTENT_LOCATIONS[i]);
    for (let j = 0; j < FIELDS.length; j++) {
      equal(parsed[FIELDS[j]], PARSED_CONTENT[i][j],
            `${CONTENT_LOCATIONS[i]} was parsed to correct ${FIELDS[j]}`);
    }
  }

  const PARSED_CHROME = [
    ["Startup::XRE_InitChildProcess", null, null, null, 456, 123, null, null],
    ["chrome://browser/content/content.js", null, null, null, 456, 123, null, null],
    ["setTimeout_timer", "foo.js", null, "resource://gre/foo.js", 123, 434, null, null],
    ["hello/<.world (jar:file://Users/mcurie/Dev/jetpacks.js)", null, null, null,
     null, null, null, null],
    ["hello/<.world", "baz.js", "bar", "http://bar/baz.js", 123, 987, "bar", null],
    ["EnterJIT", null, null, null, null, null, null, null],
  ];

  for (let i = 0; i < PARSED_CHROME.length; i++) {
    const parsed = parseLocation.apply(null, CHROME_LOCATIONS[i]);
    for (let j = 0; j < FIELDS.length; j++) {
      equal(parsed[FIELDS[j]], PARSED_CHROME[i][j],
            `${CHROME_LOCATIONS[i]} was parsed to correct ${FIELDS[j]}`);
    }
  }
});

/**
 * Takes either a string or an object and turns it into an array that
 * parseLocation.apply expects.
 */
function argify(val) {
  if (typeof val === "string") {
    return [val];
  }
  return [val.location, val.line, val.column];
}

/**
 * Takes the result of argify and turns it into an array that can be passed to
 * isContent.apply.
 */
function frameify(val) {
  return [{ location: val[0] }];
}
