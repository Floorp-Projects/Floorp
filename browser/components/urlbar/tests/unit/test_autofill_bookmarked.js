/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a specific autofill test to ensure we pick the correct bookmarked
// state of an origin. Regardless of the order of origins, we should always pick
// the correct bookmarked status.

add_task(async function () {
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
  });
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);

  let host = "example.com";
  // Add a bookmark to the http version, but ensure the https version has an
  // higher frecency.
  let bookmark = await PlacesUtils.bookmarks.insert({
    url: `http://${host}`,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits(`https://${host}`);
  }
  // ensure both fall below the threshold.
  for (let i = 0; i < 15; i++) {
    await PlacesTestUtils.addVisits(`https://not-${host}`);
  }

  async function check_autofill() {
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    let threshold = await getOriginAutofillThreshold();
    let httpOriginFrecency = await getOriginFrecency("http://", host);
    Assert.less(
      httpOriginFrecency,
      threshold,
      "Http origin frecency should be below the threshold"
    );
    let httpsOriginFrecency = await getOriginFrecency("https://", host);
    Assert.less(
      httpsOriginFrecency,
      threshold,
      "Https origin frecency should be below the threshold"
    );
    Assert.less(
      httpOriginFrecency,
      httpsOriginFrecency,
      "Http origin frecency should be below the https origin frecency"
    );

    // The http version should be filled because it's bookmarked, but with the
    // https prefix that is more frecent.
    let context = createContext("ex", { isPrivate: false });
    await check_results({
      context,
      autofilled: `${host}/`,
      completed: `https://${host}/`,
      matches: [
        makeVisitResult(context, {
          uri: `https://${host}/`,
          title: `test visit for https://${host}/`,
          heuristic: true,
        }),
        makeVisitResult(context, {
          uri: `https://not-${host}/`,
          title: `test visit for https://not-${host}/`,
        }),
      ],
    });
  }

  await check_autofill();

  // Now remove the bookmark, ensure to remove the orphans, then reinsert the
  // bookmark; thus we physically invert the order of the rows in the table.
  await checkOriginsOrder(host, ["http://", "https://"]);
  await PlacesUtils.bookmarks.remove(bookmark);
  await PlacesUtils.withConnectionWrapper("removeOrphans", async db => {
    db.execute(`DELETE FROM moz_places WHERE url = :url`, {
      url: `http://${host}/`,
    });
    db.execute(
      `DELETE FROM moz_origins WHERE prefix = "http://" AND host = :host`,
      { host }
    );
  });
  bookmark = await PlacesUtils.bookmarks.insert({
    url: `http://${host}`,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });

  await checkOriginsOrder(host, ["https://", "http://"]);

  await check_autofill();
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.remove(bookmark);
});

add_task(async function test_www() {
  // Add a bookmark to the www version
  let host = "example.com";
  await PlacesUtils.bookmarks.insert({
    url: `http://www.${host}`,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });

  info("search for start of www.");
  let context = createContext("w", { isPrivate: false });
  await check_results({
    context,
    autofilled: `www.${host}/`,
    completed: `http://www.${host}/`,
    matches: [
      makeVisitResult(context, {
        uri: `http://www.${host}/`,
        fallbackTitle: `www.${host}`,
        heuristic: true,
      }),
    ],
  });
  info("search for full www.");
  context = createContext("www.", { isPrivate: false });
  await check_results({
    context,
    autofilled: `www.${host}/`,
    completed: `http://www.${host}/`,
    matches: [
      makeVisitResult(context, {
        uri: `http://www.${host}/`,
        fallbackTitle: `www.${host}`,
        heuristic: true,
      }),
    ],
  });
  info("search for host without www.");
  context = createContext("ex", { isPrivate: false });
  await check_results({
    context,
    autofilled: `${host}/`,
    completed: `http://www.${host}/`,
    matches: [
      makeVisitResult(context, {
        uri: `http://www.${host}/`,
        fallbackTitle: `www.${host}`,
        heuristic: true,
      }),
    ],
  });
});
