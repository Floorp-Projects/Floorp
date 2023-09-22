/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a basic autofill test to ensure enabling the alternative frecency
// algorithm doesn't break autofill or tab-to-search. A more comprehensive
// testing of the algorithm itself is not included since it's something that
// may change frequently according to experimentation results.
// Other existing autofill tests will, of course, need to be adapted once an
// algorithm is promoted to be the default.

ChromeUtils.defineLazyGetter(this, "PlacesFrecencyRecalculator", () => {
  return Cc["@mozilla.org/places/frecency-recalculator;1"].getService(
    Ci.nsIObserver
  ).wrappedJSObject;
});

testEngine_setup();

add_task(async function test_autofill() {
  const origin = "example.com";
  let context = createContext(origin.substring(0, 2), { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        engineName: "Suggestions",
        heuristic: true,
      }),
    ],
  });
  // Add many visits.
  const url = `https://${origin}/`;
  await PlacesTestUtils.addVisits(new Array(10).fill(url));
  Assert.equal(
    await PlacesUtils.metadata.get("origin_alt_frecency_threshold", 0),
    0,
    "Check there's no threshold initially"
  );
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  Assert.greater(
    await PlacesUtils.metadata.get("origin_alt_frecency_threshold", 0),
    0,
    "Check a threshold has been calculated"
  );
  await check_results({
    context,
    autofilled: `${origin}/`,
    completed: url,
    matches: [
      makeVisitResult(context, {
        uri: url,
        title: `test visit for ${url}`,
        heuristic: true,
      }),
    ],
  });
  await PlacesUtils.history.clear();
});

add_task(async function test_autofill_www() {
  const origin = "example.com";
  // Add many visits.
  const url = `https://www.${origin}/`;
  await PlacesTestUtils.addVisits(new Array(10).fill(url));
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  let context = createContext(origin.substring(0, 2), { isPrivate: false });
  await check_results({
    context,
    autofilled: `${origin}/`,
    completed: url,
    matches: [
      makeVisitResult(context, {
        uri: url,
        title: `test visit for ${url}`,
        heuristic: true,
      }),
    ],
  });
  await PlacesUtils.history.clear();
});

add_task(
  {
    pref_set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 0]],
  },
  async function test_autofill_prefix_priority() {
    const origin = "localhost";
    const url = `https://${origin}/`;
    await PlacesTestUtils.addVisits([url, `http://${origin}/`]);
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

    let engine = Services.search.defaultEngine;
    let context = createContext(origin.substring(0, 2), { isPrivate: false });
    await check_results({
      context,
      autofilled: `${origin}/`,
      completed: url,
      matches: [
        makeVisitResult(context, {
          uri: url,
          title: `test visit for ${url}`,
          heuristic: true,
        }),
        makeSearchResult(context, {
          engineName: engine.name,
          engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
          uri: UrlbarUtils.stripPublicSuffixFromHost(engine.searchUrlDomain),
          providesSearchMode: true,
          query: "",
          providerName: "TabToSearch",
        }),
      ],
    });
    await PlacesUtils.history.clear();
  }
);

add_task(async function test_autofill_threshold() {
  await PlacesTestUtils.addVisits(new Array(10).fill("https://example.com/"));
  // Add more visits to the same origins to differenciate the frecency scores.
  await PlacesTestUtils.addVisits([
    "https://example.com/2",
    "https://example.com/3",
  ]);
  await PlacesTestUtils.addVisits("https://somethingelse.org/");
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  let threshold = await PlacesUtils.metadata.get(
    "origin_alt_frecency_threshold",
    0
  );
  Assert.greater(
    threshold,
    await PlacesTestUtils.getDatabaseValue("moz_origins", "alt_frecency", {
      host: "somethingelse.org",
    }),
    "Check mozilla.org has a lower frecency than the threshold"
  );
  Assert.equal(
    threshold,
    await PlacesTestUtils.getDatabaseValue("moz_origins", "avg(alt_frecency)"),
    "Check the threshold has been calculared correctly"
  );

  let engine = Services.search.defaultEngine;
  let context = createContext("so", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query: "so",
        engineName: engine.name,
      }),
      makeVisitResult(context, {
        uri: "https://somethingelse.org/",
        title: "test visit for https://somethingelse.org/",
      }),
    ],
  });
  await PlacesUtils.history.clear();
});

add_task(async function test_autofill_cutoff() {
  // Add many visits older than the default 90 days cutoff.
  const visitDate = new Date(Date.now() - 120 * 86400000);
  await PlacesTestUtils.addVisits(
    new Array(10).fill("https://example.com/").map(url => ({ url, visitDate }))
  );
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  Assert.strictEqual(
    await PlacesTestUtils.getDatabaseValue("moz_origins", "alt_frecency", {
      host: "example.com",
    }),
    null,
    "Check example.com has a NULL frecency"
  );

  let engine = Services.search.defaultEngine;
  let context = createContext("ex", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query: "ex",
        engineName: engine.name,
      }),
      makeVisitResult(context, {
        uri: "https://example.com/",
        title: "test visit for https://example.com/",
      }),
    ],
  });
  await PlacesUtils.history.clear();
});

add_task(async function test_autofill_threshold_www() {
  // Only one visit to the non-www origin, many to the www. version. We expect
  // example.com to autofill even if its frecency is small, because the overall
  // frecency for both origins should be considered.
  await PlacesTestUtils.addVisits("https://example.com/");
  await PlacesTestUtils.addVisits(
    new Array(10).fill("https://www.example.com/")
  );
  await PlacesTestUtils.addVisits(
    new Array(10).fill("https://www.somethingelse.org/")
  );
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  let threshold = await PlacesUtils.metadata.get(
    "origin_alt_frecency_threshold",
    0
  );
  let frecencyOfExampleCom = await PlacesTestUtils.getDatabaseValue(
    "moz_origins",
    "alt_frecency",
    {
      host: "example.com",
    }
  );
  let frecencyOfWwwExampleCom = await PlacesTestUtils.getDatabaseValue(
    "moz_origins",
    "alt_frecency",
    {
      host: "www.example.com",
    }
  );
  Assert.greater(
    threshold,
    frecencyOfExampleCom,
    "example.com frecency is lower than the threshold"
  );
  Assert.greater(
    frecencyOfWwwExampleCom,
    threshold,
    "www.example.com frecency is higher than the threshold"
  );

  // We used to wrongly use the average between the 2 domains, so check also
  // the average would not autofill.
  Assert.greater(
    threshold,
    [frecencyOfExampleCom, frecencyOfWwwExampleCom].reduce(
      (acc, v, i, arr) => acc + v / arr.length,
      0
    ),
    "Check frecency average is lower than the threshold"
  );

  let context = createContext("ex", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/",
    completed: "https://www.example.com/",
    matches: [
      makeVisitResult(context, {
        uri: "https://www.example.com/",
        title: "test visit for https://www.example.com/",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://example.com/",
        title: "test visit for https://example.com/",
      }),
    ],
  });
  await PlacesUtils.history.clear();
});
