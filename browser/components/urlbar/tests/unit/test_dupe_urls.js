/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure inline autocomplete doesn't return zero frecency pages.

add_task(async function setup() {
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.urlbar.suggest.searches");
  });

  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
});

add_task(async function test_dupe_urls() {
  info("Searching for urls with dupes should only show one");
  await PlacesTestUtils.addVisits(
    {
      uri: Services.io.newURI("http://mozilla.org/"),
    },
    {
      uri: Services.io.newURI("http://mozilla.org/?"),
    }
  );
  let context = createContext("moz", { isPrivate: false });
  await check_results({
    context,
    autofilled: "mozilla.org/",
    completed: "http://mozilla.org/",
    matches: [
      makeVisitResult(context, {
        uri: "http://mozilla.org/",
        title: "mozilla.org",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

add_task(async function test_dupe_secure_urls() {
  await PlacesTestUtils.addVisits(
    {
      uri: Services.io.newURI("https://example.org/"),
    },
    {
      uri: Services.io.newURI("https://example.org/?"),
    }
  );
  let context = createContext("exam", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.org/",
    completed: "https://example.org/",
    matches: [
      makeVisitResult(context, {
        uri: "https://example.org/",
        title: "https://example.org",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});
