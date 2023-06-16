/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test source mappings WASM sources.
// This test is quite general and test various functions.

const {
  WasmRemap,
} = require("resource://devtools/client/shared/source-map-loader/utils/wasmRemap.js");
const {
  SourceMapConsumer,
} = require("resource://devtools/client/shared/vendor/source-map/source-map.js");

SourceMapConsumer.initialize({
  "lib/mappings.wasm":
    "resource://devtools/client/shared/vendor/source-map/lib/mappings.wasm",
});

add_task(async function smokeTest() {
  const testMap1 = {
    version: 3,
    file: "min.js",
    names: [],
    sources: ["one.js", "two.js"],
    sourceRoot: "/the/root",
    mappings: "CAAC,IAAM,SACU,GAAC",
  };
  const testMap1Entries = [
    { offset: 1, line: 1, column: 1 },
    { offset: 5, line: 1, column: 7 },
    { offset: 14, line: 2, column: 17 },
    { offset: 17, line: 2, column: 18 },
  ];

  const map1 = await new SourceMapConsumer(testMap1);

  const remap1 = new WasmRemap(map1);

  is(remap1.file, "min.js");
  is(remap1.hasContentsOfAllSources(), false);
  is(remap1.sources.length, 2);
  is(remap1.sources[0], "/the/root/one.js");
  is(remap1.sources[1], "/the/root/two.js");

  const expectedEntries = testMap1Entries.slice(0);
  remap1.eachMapping(function (entry) {
    const expected = expectedEntries.shift();
    is(entry.generatedLine, expected.offset);
    is(entry.generatedColumn, 0);
    is(entry.originalLine, expected.line);
    is(entry.originalColumn, expected.column);
    is(entry.name, null);
  });

  const pos1 = remap1.originalPositionFor({ line: 5, column: 0 });
  is(pos1.line, 1);
  is(pos1.column, 7);
  is(pos1.source, "/the/root/one.js");

  const pos2 = remap1.generatedPositionFor({
    source: "/the/root/one.js",
    line: 2,
    column: 18,
  });
  is(pos2.line, 17);
  is(pos2.column, 0);
  is(pos2.lastColumn, undefined);

  remap1.computeColumnSpans();
  const pos3 = remap1.allGeneratedPositionsFor({
    source: "/the/root/one.js",
    line: 2,
    column: 17,
  });
  is(pos3.length, 1);
  is(pos3[0].line, 14);
  is(pos3[0].column, 0);
  is(pos3[0].lastColumn, 0);

  map1.destroy();
});

add_task(async function contentPresents() {
  const testMap2 = {
    version: 3,
    file: "none.js",
    names: [],
    sources: ["zero.js"],
    mappings: "",
    sourcesContent: ["//test"],
  };

  const map2 = await new SourceMapConsumer(testMap2);
  const remap2 = new WasmRemap(map2);
  is(remap2.file, "none.js");
  ok(remap2.hasContentsOfAllSources());
  is(remap2.sourceContentFor("zero.js"), "//test");

  map2.destroy();
});

add_task(async function readAndTransposeWasmMap() {
  const source = {
    id: "wasm.js",
    sourceMapBaseURL: "wasm:http://example.com/whatever/:min.js",
    sourceMapURL: `${URL_ROOT}fixtures/wasm.js.map`,
    isWasm: true,
  };

  const urls = await gSourceMapLoader.getOriginalURLs(source);
  Assert.deepEqual(urls, [
    {
      id: "wasm.js/originalSource-63954a1c231200652c0d99c6a69cd178",
      url: `${URL_ROOT}fixtures/one.js`,
    },
  ]);

  const { line, column } = await gSourceMapLoader.getOriginalLocation({
    sourceId: source.id,
    line: 5,
  });
  is(line, 1);
  is(column, 7);
});
