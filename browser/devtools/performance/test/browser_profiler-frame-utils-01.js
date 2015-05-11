/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that frame-utils isContent and parseLocation work as intended
 * when parsing over frames from the profiler.
 */

const CONTENT_LOCATIONS = [
  "hello/<.world (https://foo/bar.js:123:987)",
  "hello/<.world (http://foo/bar.js:123:987)",
  "hello/<.world (http://foo/bar.js:123)",
  "hello/<.world (http://foo/bar.js#baz:123:987)",
  "hello/<.world (http://foo/#bar:123:987)",
  "hello/<.world (http://foo/:123:987)",
  "hello/<.world (app://myfxosapp/file.js:100:1)",
].map(argify);

const CHROME_LOCATIONS = [
  { location: "Startup::XRE_InitChildProcess", line: 456, column: 123 },
  { location: "chrome://browser/content/content.js", line: 456, column: 123 },
  "setTimeout_timer (resource://gre/foo.js:123:434)",
  "hello/<.world (jar:file://Users/mcurie/Dev/jetpacks.js)",
  "hello/<.world (resource://foo.js -> http://bar/baz.js:123:987)",
  "EnterJIT",
].map(argify);

function test() {
  const { isContent, parseLocation } = devtools.require("devtools/shared/profiler/frame-utils");

  for (let frame of CONTENT_LOCATIONS) {
    ok(isContent.apply(null, frameify(frame)), `${frame[0]} should be considered a content frame.`);
  }

  for (let frame of CHROME_LOCATIONS) {
    ok(!isContent.apply(null, frameify(frame)), `${frame[0]} should not be considered a content frame.`);
  }

  // functionName, fileName, hostName, url, line, column
  const FIELDS = ["functionName", "fileName", "hostName", "url", "line", "column"];
  const PARSED_CONTENT = [
    ["hello/<.world", "bar.js", "foo", "https://foo/bar.js", 123, 987],
    ["hello/<.world", "bar.js", "foo", "http://foo/bar.js", 123, 987],
    ["hello/<.world", "bar.js", "foo", "http://foo/bar.js", 123, null],
    ["hello/<.world", "bar.js#baz", "foo", "http://foo/bar.js#baz", 123, 987],
    ["hello/<.world", "#bar", "foo", "http://foo/#bar", 123, 987],
    ["hello/<.world", "/", "foo", "http://foo/", 123, 987],
    ["hello/<.world", "file.js", "myfxosapp", "app://myfxosapp/file.js", 100, 1],
  ];

  for (let i = 0; i < PARSED_CONTENT.length; i++) {
    let parsed = parseLocation.apply(null, CONTENT_LOCATIONS[i]);
    for (let j = 0; j < FIELDS.length; j++) {
      is(parsed[FIELDS[j]], PARSED_CONTENT[i][j], `${CONTENT_LOCATIONS[i]} was parsed to correct ${FIELDS[j]}`);
    }
  }

  const PARSED_CHROME = [
    ["Startup::XRE_InitChildProcess", null, null, null, 456, 123],
    ["chrome://browser/content/content.js", null, null, null, 456, 123],
    ["setTimeout_timer", "foo.js", null, "resource://gre/foo.js", 123, 434],
    ["hello/<.world (jar:file://Users/mcurie/Dev/jetpacks.js)", null, null, null, null, null],
    ["hello/<.world", "baz.js", "bar", "http://bar/baz.js", 123, 987],
    ["EnterJIT", null, null, null, null, null],
  ];

  for (let i = 0; i < PARSED_CHROME.length; i++) {
    let parsed = parseLocation.apply(null, CHROME_LOCATIONS[i]);
    for (let j = 0; j < FIELDS.length; j++) {
      is(parsed[FIELDS[j]], PARSED_CHROME[i][j], `${CHROME_LOCATIONS[i]} was parsed to correct ${FIELDS[j]}`);
    }
  }

  finish();
}

/**
 * Takes either a string or an object and turns it into an array that
 * parseLocation.apply expects.
 */
function argify (val) {
  if (typeof val === "string") {
    return [val];
  } else {
    return [val.location, val.line, val.column];
  }
}

/**
 * Takes the result of argify and turns it into an array that can be passed to
 * isContent.apply.
 */
function frameify(val) {
  return [{ location: val[0] }];
}
