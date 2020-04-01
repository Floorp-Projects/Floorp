/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Use initOrigin to test the operation of the origin parser on a list of URLs
// we should support. If the origin doesn't parse, then initOrigin will throw an
// exception (and potentially MOZ_ASSERT under debug builds). Handling of
// obsolete or invalid origins is handled in other test files.
async function testSteps() {
  const basePath = "storage/default/";
  const longExampleOriginSubstring = "a".repeat(
    255 - "https://example..com".length
  );
  const origins = [
    // General
    {
      dirName: "https+++example.com",
      url: "https://example.com",
    },
    {
      dirName: "https+++smaug----.github.io",
      url: "https://smaug----.github.io/",
    },
    // About
    {
      dirName: "about+home",
      url: "about:home",
    },
    {
      dirName: "about+reader",
      url: "about:reader",
    },
    // IPv6
    {
      dirName: "https+++[++]",
      url: "https://[::]",
    },
    {
      dirName: "https+++[ffff+ffff+ffff+ffff+ffff+ffff+ffff+ffff]",
      url: "https://[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]",
    },
    {
      dirName: "http+++[2010+836b+4179++836b+4179]",
      url: "http://[2010:836B:4179::836B:4179]:80",
    },
    {
      dirName: "https+++[++ffff+8190+3426]",
      url: "https://[::FFFF:129.144.52.38]",
    },
    // MAX_PATH on Windows (260); storage/default/https+++example.{a....a}.com
    // should have already exceeded the MAX_PATH limitation on Windows.
    // There is a limitation (255) for each component on Windows so that we can
    // only let the component be 255 chars and expect the wwhole path to be
    // greater then 260.
    {
      dirName: `https+++example.${longExampleOriginSubstring}.com`,
      url: `https://example.${longExampleOriginSubstring}.com`,
    },
    // EndingWithPeriod
    {
      dirName: "https+++example.com.",
      url: "https://example.com.",
    },
  ];

  for (let origin of origins) {
    info(`Testing ${origin.url}`);

    try {
      let request = initStorageAndOrigin(getPrincipal(origin.url), "default");
      await requestFinished(request);

      ok(true, "Should not have thrown");
    } catch (ex) {
      ok(false, "Should not have thrown");
    }

    let dir = getRelativeFile(basePath + origin.dirName);
    ok(dir.exists(), "Origin was created");
    ok(
      origin.dirName === dir.leafName,
      `Origin ${origin.dirName} was created expectedly`
    );
  }

  let request = clear();
  await requestFinished(request);
}
