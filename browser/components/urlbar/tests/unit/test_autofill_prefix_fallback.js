/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests autofill prefix fallback in case multiple origins have the same
// exact frecency.
// We should prefer https, or in case of other prefixes just sort by descending
// id.

add_task(async function() {
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
  });
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);

  let host = "example.com";
  let prefixes = ["https://", "https://www.", "http://", "http://www."];
  for (let prefix of prefixes) {
    await PlacesUtils.bookmarks.insert({
      url: `${prefix}${host}`,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    });
  }
  await checkOriginsOrder(host, prefixes);

  // The https://www version should be filled because it's https and the www
  // version has been added later so it has an higher id.
  let context = createContext("ex", { isPrivate: false });
  await check_results({
    context,
    autofilled: `${host}/`,
    completed: `https://www.${host}/`,
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: `https://www.${host}/`,
        title: `https://www.${host}`,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: `https://${host}/`,
        title: `${host}`,
      }),
    ],
  });

  // Remove and reinsert bookmarks in another order.
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
  prefixes = ["https://www.", "http://", "https://", "http://www."];
  for (let prefix of prefixes) {
    await PlacesUtils.bookmarks.insert({
      url: `${prefix}${host}`,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    });
  }
  await checkOriginsOrder(host, prefixes);

  await check_results({
    context,
    autofilled: `${host}/`,
    completed: `https://${host}/`,
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: `https://${host}/`,
        title: `https://${host}`,
        heuristic: true,
      }),
      makeBookmarkResult(context, {
        uri: `https://www.${host}/`,
        title: `www.${host}`,
      }),
    ],
  });
});
