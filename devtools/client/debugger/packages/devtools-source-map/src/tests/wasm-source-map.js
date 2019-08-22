/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test WasmRemap

const { WasmRemap } = require("../utils/wasmRemap");
const { createConsumer } = require("../utils/createConsumer");

jest.mock("devtools-utils/src/network-request");

describe("wasm source maps", () => {
  test("smoke test", async () => {
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

    const map1 = await createConsumer(testMap1);
    const remap1 = new WasmRemap(map1);

    expect(remap1.file).toEqual("min.js");
    expect(remap1.hasContentsOfAllSources()).toEqual(false);
    expect(remap1.sources).toHaveLength(2);
    expect(remap1.sources[0]).toEqual("/the/root/one.js");
    expect(remap1.sources[1]).toEqual("/the/root/two.js");

    const expectedEntries = testMap1Entries.slice(0);
    remap1.eachMapping(function(entry) {
      const expected = expectedEntries.shift();
      expect(entry.generatedLine).toEqual(expected.offset);
      expect(entry.generatedColumn).toEqual(0);
      expect(entry.originalLine).toEqual(expected.line);
      expect(entry.originalColumn).toEqual(expected.column);
      expect(entry.name).toEqual(null);
    });

    const pos1 = remap1.originalPositionFor({ line: 5, column: 0 });
    expect(pos1.line).toEqual(1);
    expect(pos1.column).toEqual(7);
    expect(pos1.source).toEqual("/the/root/one.js");

    const pos2 = remap1.generatedPositionFor({
      source: "/the/root/one.js",
      line: 2,
      column: 18,
    });
    expect(pos2.line).toEqual(17);
    expect(pos2.column).toEqual(0);
    expect(pos2.lastColumn).toEqual(undefined);

    remap1.computeColumnSpans();
    const pos3 = remap1.allGeneratedPositionsFor({
      source: "/the/root/one.js",
      line: 2,
      column: 17,
    });
    expect(pos3).toHaveLength(1);
    expect(pos3[0].line).toEqual(14);
    expect(pos3[0].column).toEqual(0);
    expect(pos3[0].lastColumn).toEqual(0);
  });

  test("content presents", async () => {
    const testMap2 = {
      version: 3,
      file: "none.js",
      names: [],
      sources: ["zero.js"],
      mappings: "",
      sourcesContent: ["//test"],
    };

    const map2 = await createConsumer(testMap2);
    const remap2 = new WasmRemap(map2);
    expect(remap2.file).toEqual("none.js");
    expect(remap2.hasContentsOfAllSources()).toEqual(true);
    expect(remap2.sourceContentFor("zero.js")).toEqual("//test");
  });

  test("read and transpose wasm map", async () => {
    const source = {
      id: "min.js",
      url: "wasm:http://example.com/whatever/:min.js",
      sourceMapURL: "http://example.com/whatever/min.js.map",
      isWasm: true,
    };

    require("devtools-utils/src/network-request").mockImplementationOnce(() => {
      const content = JSON.stringify({
        version: 3,
        file: "min.js",
        names: [],
        sources: ["one.js"],
        mappings: "CAAC,IAAM",
      });
      return { content };
    });

    const { getOriginalURLs, getOriginalLocation } = require("../source-map");

    const urls = await getOriginalURLs(source);
    expect(urls).toEqual(["http://example.com/whatever/one.js"]);

    const { line, column } = await getOriginalLocation({
      sourceId: source.id,
      line: 5,
    });
    expect(line).toEqual(1);
    expect(column).toEqual(7);
  });
});
