/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test WasmRemap

const { WasmRemap } = require("devtools/shared/wasm-source-map");
const { SourceMapConsumer } = require("source-map");

const testMap1 = {
  version: 3,
  file: "min.js",
  names: [],
  sources: ["one.js", "two.js"],
  sourceRoot: "/the/root",
  mappings: "CAAC,IAAM,SACU,GAAC"
};
const testMap1Entries = [
  { offset: 1, line: 1, column: 1 },
  { offset: 5, line: 1, column: 7 },
  { offset: 14, line: 2, column: 17 },
  { offset: 17, line: 2, column: 18 },
];
const testMap2 = {
  version: 3,
  file: "none.js",
  names: [],
  sources: ["zero.js"],
  mappings: "",
  sourcesContent: ["//test"]
};

function run_test() {
  const map1 = new SourceMapConsumer(testMap1);
  const remap1 = new WasmRemap(map1);

  equal(remap1.file, "min.js");
  equal(remap1.hasContentsOfAllSources(), false);
  equal(remap1.sources.length, 2);
  equal(remap1.sources[0], "/the/root/one.js");
  equal(remap1.sources[1], "/the/root/two.js");

  const expectedEntries = testMap1Entries.slice(0);
  remap1.eachMapping(function(entry) {
    const expected = expectedEntries.shift();
    equal(entry.generatedLine, expected.offset);
    equal(entry.generatedColumn, 0);
    equal(entry.originalLine, expected.line);
    equal(entry.originalColumn, expected.column);
    equal(entry.name, null);
  });

  const pos1 = remap1.originalPositionFor({line: 5, column: 0});
  equal(pos1.line, 1);
  equal(pos1.column, 7);
  equal(pos1.source, "/the/root/one.js");

  const pos2 = remap1.generatedPositionFor({
    source: "/the/root/one.js",
    line: 2,
    column: 18
  });
  equal(pos2.line, 17);
  equal(pos2.column, 0);
  equal(pos2.lastColumn, undefined);

  remap1.computeColumnSpans();
  const pos3 = remap1.allGeneratedPositionsFor({
    source: "/the/root/one.js",
    line: 2,
    column: 17
  });
  equal(pos3.length, 1);
  equal(pos3[0].line, 14);
  equal(pos3[0].column, 0);
  equal(pos3[0].lastColumn, Infinity);

  const map2 = new SourceMapConsumer(testMap2);
  const remap2 = new WasmRemap(map2);
  equal(remap2.file, "none.js");
  equal(remap2.hasContentsOfAllSources(), true);
  equal(remap2.sourceContentFor("zero.js"), "//test");
}
