/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the functionality of ScriptStore.

const ScriptStore = require("devtools/server/actors/utils/ScriptStore");

// Fixtures

const firstSource = "firstSource";
const secondSource = "secondSource";
const thirdSource = "thirdSource";

const scripts = new Set([
  {
    url: "a.js",
    source: firstSource,
    startLine: 1,
    lineCount: 100,
    global: "g1"
  },
  {
    url: "a.js",
    source: firstSource,
    startLine: 1,
    lineCount: 40,
    global: "g1"
  },
  {
    url: "a.js",
    source: firstSource,
    startLine: 50,
    lineCount: 100,
    global: "g1"
  },
  {
    url: "a.js",
    source: firstSource,
    startLine: 60,
    lineCount: 90,
    global: "g1"
  },
  {
    url: "index.html",
    source: secondSource,
    startLine: 150,
    lineCount: 1,
    global: "g2"
  },
  {
    url: "index.html",
    source: thirdSource,
    startLine: 200,
    lineCount: 100,
    global: "g2"
  },
  {
    url: "index.html",
    source: thirdSource,
    startLine: 250,
    lineCount: 10,
    global: "g2"
  },
  {
    url: "index.html",
    source: thirdSource,
    startLine: 275,
    lineCount: 5,
    global: "g2"
  }
]);

function contains(script, line) {
  return script.startLine <= line &&
    line < script.startLine + script.lineCount;
}

function run_test() {
  testAddScript();
  testAddScripts();
  testGetSources();
  testGetScriptsBySource();
  testGetScriptsBySourceAndLine();
  testGetScriptsByURL();
  testGetScriptsByURLAndLine();
}

function testAddScript() {
  const ss = new ScriptStore();

  for (let s of scripts) {
    ss.addScript(s);
  }

  equal(ss.getAllScripts().length, scripts.size);

  for (let s of ss.getAllScripts()) {
    ok(scripts.has(s));
  }
}

function testAddScripts() {
  const ss = new ScriptStore();
  ss.addScripts([...scripts]);

  equal(ss.getAllScripts().length, scripts.size);

  for (let s of ss.getAllScripts()) {
    ok(scripts.has(s));
  }
}

function testGetSources() {
  const ss = new ScriptStore();
  ss.addScripts([...scripts]);

  const expected = new Set([firstSource, secondSource, thirdSource]);
  const actual = ss.getSources();
  equal(expected.size, actual.length);

  for (let s of actual) {
    ok(expected.has(s));
    expected.delete(s);
  }
}

function testGetScriptsBySource() {
  const ss = new ScriptStore();
  ss.addScripts([...scripts]);

  const expected = [...scripts].filter(s => s.source === thirdSource);
  const actual = ss.getScriptsBySource(thirdSource);

  deepEqual(actual, expected);
}

function testGetScriptsBySourceAndLine() {
  const ss = new ScriptStore();
  ss.addScripts([...scripts]);

  const expected = [...scripts].filter(
    s => s.source === firstSource && contains(s, 65));
  const actual = ss.getScriptsBySourceAndLine(firstSource, 65);

  deepEqual(actual, expected);
}

function testGetScriptsByURL() {
  const ss = new ScriptStore();
  ss.addScripts([...scripts]);

  const expected = [...scripts].filter(s => s.url === "index.html");
  const actual = ss.getScriptsByURL("index.html");

  deepEqual(actual, expected);
}

function testGetScriptsByURLAndLine() {
  const ss = new ScriptStore();
  ss.addScripts([...scripts]);

  const expected = [...scripts].filter(
    s => s.url === "index.html" && contains(s, 250));
  const actual = ss.getScriptsByURLAndLine("index.html", 250);

  deepEqual(actual, expected);
}
