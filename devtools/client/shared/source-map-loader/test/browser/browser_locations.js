/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Covert getOriginalLocation and getGeneratedLocation functions.

add_task(async function testGetOriginalLocation() {
  await fetchFixtureSourceMap("bundle");

  const generatedLocation = {
    sourceId: "bundle.js",
    line: 49,
  };

  const originalLocation = await gSourceMapLoader.getOriginalLocation(
    generatedLocation
  );
  Assert.deepEqual(
    originalLocation,
    {
      column: 0,
      line: 3,
      sourceId: "bundle.js/originalSource-fe2c41d3535b76c158e39ba4f3ff826a",
      sourceUrl: "webpack:///entry.js",
    },
    "Mapped a generated location"
  );

  const originalLocation2 = await gSourceMapLoader.getOriginalLocation(
    originalLocation
  );
  Assert.deepEqual(originalLocation2, null, "No mapped location");

  gSourceMapLoader.clearSourceMaps();
  const originalLocation3 = await gSourceMapLoader.getOriginalLocation(
    generatedLocation
  );
  Assert.deepEqual(
    originalLocation3,
    null,
    "after clearing the source maps, the same generated location no longer maps"
  );
});

add_task(async function testGetGeneratedLocation() {
  await fetchFixtureSourceMap("bundle");

  const originalLocation = {
    column: 0,
    line: 3,
    sourceId: "bundle.js/originalSource-fe2c41d3535b76c158e39ba4f3ff826a",
  };

  const source = {
    url: "webpack:///entry.js",
    id: "bundle.js/originalSource-fe2c41d3535b76c158e39ba4f3ff826a",
  };

  const generatedLocation = await gSourceMapLoader.getGeneratedLocation(
    originalLocation,
    source
  );
  Assert.deepEqual(
    generatedLocation,
    {
      sourceId: "bundle.js",
      line: 49,
      column: 0,
    },
    "Map an original location"
  );

  {
    gSourceMapLoader.clearSourceMaps();

    const secondGeneratedLocation = await gSourceMapLoader.getGeneratedLocation(
      originalLocation,
      source
    );
    Assert.deepEqual(
      secondGeneratedLocation,
      null,
      "after clearing source maps, the same location no longer maps to an original location"
    );
  }

  {
    // we expect symmetric mappings, which means that if
    // we map a generated location to an original location,
    // and then map it back, we should get the original generated value.
    // e.g. G[8, 0] -> O[5, 4] -> G[8, 0]
    await fetchFixtureSourceMap("if.out");

    const genLoc1 = {
      sourceId: "if.out.js",
      column: 0,
      line: 8,
    };

    const ifSource = {
      url: "if.js",
      id: "if.out.js/originalSource-5ad3141023dae912c5f8833c7e03beeb",
    };

    const oLoc = await gSourceMapLoader.getOriginalLocation(genLoc1);
    const genLoc2 = await gSourceMapLoader.getGeneratedLocation(oLoc, ifSource);

    Assert.deepEqual(genLoc2, genLoc1, "location mapping is symmetric");
  }

  {
    // we expect that an undefined column will be handled like a
    // location w/ column 0. e.g. G[8, u] -> O[5, 4] -> G[8, 0]
    await fetchFixtureSourceMap("if.out");

    const genLoc1 = {
      sourceId: "if.out.js",
      column: undefined,
      line: 8,
    };

    const ifSource = {
      url: "if.js",
      id: "if.out.js/originalSource-5ad3141023dae912c5f8833c7e03beeb",
    };

    const oLoc = await gSourceMapLoader.getOriginalLocation(genLoc1);
    const genLoc2 = await gSourceMapLoader.getGeneratedLocation(oLoc, ifSource);

    Assert.deepEqual(
      genLoc2,
      {
        sourceId: "if.out.js",
        column: 0,
        line: 8,
      },
      "undefined column is handled like 0 column"
    );
  }
});
