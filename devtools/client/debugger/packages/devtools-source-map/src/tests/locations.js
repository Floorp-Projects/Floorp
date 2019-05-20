/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

jest.mock("devtools-utils/src/network-request");
const {
  getOriginalLocation,
  getGeneratedLocation,
  clearSourceMaps,
} = require("../source-map");

const { setupBundleFixture } = require("./helpers");

describe("getOriginalLocation", () => {
  beforeEach(() => {
    clearSourceMaps();
  });

  test("maps a generated location", async () => {
    await setupBundleFixture("bundle");
    const location = {
      sourceId: "bundle.js",
      line: 49,
    };

    const originalLocation = await getOriginalLocation(location);
    expect(originalLocation).toEqual({
      column: 0,
      line: 3,
      sourceId: "bundle.js/originalSource-fe2c41d3535b76c158e39ba4f3ff826a",
      sourceUrl: "webpack:///entry.js",
    });
  });

  test("does not map an original location", async () => {
    const location = {
      column: 0,
      line: 3,
      sourceId: "bundle.js/originalSource-fe2c41d3535b76c158e39ba4f3ff826a",
      sourceUrl: "webpack:///entry.js",
    };
    const originalLocation = await getOriginalLocation(location);
    expect(originalLocation).toEqual(originalLocation);
  });
});

describe("getGeneratedLocation", () => {
  beforeEach(() => {
    clearSourceMaps();
  });

  test("maps an original location", async () => {
    await setupBundleFixture("bundle");
    const location = {
      column: 0,
      line: 3,
      sourceId: "bundle.js/originalSource-fe2c41d3535b76c158e39ba4f3ff826a",
    };

    const source = {
      url: "webpack:///entry.js",
      id: "bundle.js/originalSource-fe2c41d3535b76c158e39ba4f3ff826a",
    };

    const generatedLocation = await getGeneratedLocation(location, source);
    expect(generatedLocation).toEqual({
      sourceId: "bundle.js",
      line: 49,
      column: 0,
    });
  });

  test("location mapping is symmetric", async () => {
    // we expect symmetric mappings, which means that if
    // we map a generated location to an original location,
    // and then map it back, we should get the original generated value.
    // e.g. G[8, 0] -> O[5, 4] -> G[8, 0]

    await setupBundleFixture("if.out");

    const genLoc1 = {
      sourceId: "if.out.js",
      column: 0,
      line: 8,
    };

    const ifSource = {
      url: "if.js",
      id: "if.out.js/originalSource-5ad3141023dae912c5f8833c7e03beeb",
    };

    const oLoc = await getOriginalLocation(genLoc1);
    const genLoc2 = await getGeneratedLocation(oLoc, ifSource);

    expect(genLoc2).toEqual({
      sourceId: "if.out.js",
      column: 0,
      line: 8,
    });
  });

  test("undefined column is handled like 0 column", async () => {
    // we expect that an undefined column will be handled like a
    // location w/ column 0. e.g. G[8, u] -> O[5, 4] -> G[8, 0]

    await setupBundleFixture("if.out");

    const genLoc1 = {
      sourceId: "if.out.js",
      column: undefined,
      line: 8,
    };

    const ifSource = {
      url: "if.js",
      id: "if.out.js/originalSource-5ad3141023dae912c5f8833c7e03beeb",
    };

    const oLoc = await getOriginalLocation(genLoc1);
    const genLoc2 = await getGeneratedLocation(oLoc, ifSource);
    // console.log(formatLocations([genLoc1, oLoc, genLoc2]));

    expect(genLoc2).toEqual({
      sourceId: "if.out.js",
      column: 0,
      line: 8,
    });
  });

  test("does not map an original location", async () => {
    const location = {
      column: 0,
      line: 3,
      sourceId: "bundle.js/originalSource-fe2c41d3535b76c158e39ba4f3ff826a",
      sourceUrl: "webpack:///entry.js",
    };

    const source = {
      url: "webpack:///entry.js",
      id: "bundle.js/originalSource-fe2c41d3535b76c158e39ba4f3ff826a",
    };

    const generatedLocation = await getGeneratedLocation(location, source);
    expect(generatedLocation).toEqual(generatedLocation);
  });
});
