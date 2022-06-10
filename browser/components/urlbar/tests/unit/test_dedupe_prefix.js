/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing that we dedupe results that have the same URL and title as another
// except for their prefix (e.g. http://www.).
add_task(async function dedupe_prefix() {
  // We need to set the title or else we won't dedupe. We only dedupe when
  // titles match up to mitigate deduping when the www. version of a site is
  // completely different from it's www-less counterpart and thus presumably
  // has a different title.
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/foo/",
      title: "Example Page",
    },
    {
      uri: "http://www.example.com/foo/",
      title: "Example Page",
    },
    {
      uri: "https://example.com/foo/",
      title: "Example Page",
    },
    // Note that we add https://www.example.com/foo/ twice here.
    {
      uri: "https://www.example.com/foo/",
      title: "Example Page",
    },
    {
      uri: "https://www.example.com/foo/",
      title: "Example Page",
    },
  ]);

  // Expected results:
  //
  // Autofill result:
  //   https://www.example.com has the highest origin frecency since we added 2
  //   visits to https://www.example.com/foo/ and only one visit to the other
  //   URLs.
  // Other results:
  //   https://example.com/foo/ has the highest possible prefix rank, and it
  //   does not dupe the autofill result, so only it should be included.
  let context = createContext("example.com/foo/", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/foo/",
    completed: "https://www.example.com/foo/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://www.example.com/foo/",
        title: "Example Page",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://example.com/foo/",
        title: "Example Page",
      }),
    ],
  });

  // Add more visits to the lowest-priority prefix. It should be the heuristic
  // result but we should still show our highest-priority result. https://www.
  // should not appear at all.
  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://www.example.com/foo/",
        title: "Example Page",
      },
    ]);
  }

  // Expected results:
  //
  // Autofill result:
  //   http://www.example.com now has the highest origin frecency since we added
  //   4 visits to http://www.example.com/foo/
  // Other results:
  //   Same as before
  context = createContext("example.com/foo/", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/foo/",
    completed: "http://www.example.com/foo/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://www.example.com/foo/",
        title: "Example Page",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://example.com/foo/",
        title: "Example Page",
      }),
    ],
  });

  // Add enough https:// vists for it to have the highest frecency. It should
  // be the heuristic result. We should still get the https://www. result
  // because we still show results with the same key and protocol if they differ
  // from the heuristic result in having www.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([
      {
        uri: "https://example.com/foo/",
        title: "Example Page",
      },
    ]);
  }

  // Expected results:
  //
  // Autofill result:
  //   https://example.com now has the highest origin frecency since we added
  //   6 visits to https://example.com/foo/
  // Other results:
  //   https://example.com/foo/ has the highest possible prefix rank, but it
  //   dupes the heuristic so it should not be included.
  //   https://www.example.com/foo/ has the next highest prefix rank, and it
  //   does not dupe the heuristic, so only it should be included.
  context = createContext("example.com/foo/", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/foo/",
    completed: "https://example.com/foo/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://example.com/foo/",
        title: "Example Page",
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});

// This is the same as the previous task but with `experimental.hideHeuristic`
// enabled.
add_task(async function hideHeuristic() {
  UrlbarPrefs.set("experimental.hideHeuristic", true);

  // We need to set the title or else we won't dedupe. We only dedupe when
  // titles match up to mitigate deduping when the www. version of a site is
  // completely different from it's www-less counterpart and thus presumably
  // has a different title.
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/foo/",
      title: "Example Page",
    },
    {
      uri: "http://www.example.com/foo/",
      title: "Example Page",
    },
    {
      uri: "https://example.com/foo/",
      title: "Example Page",
    },
    // Note that we add https://www.example.com/foo/ twice here.
    {
      uri: "https://www.example.com/foo/",
      title: "Example Page",
    },
    {
      uri: "https://www.example.com/foo/",
      title: "Example Page",
    },
  ]);

  // Expected results:
  //
  // Autofill result:
  //   https://www.example.com has the highest origin frecency since we added 2
  //   visits to https://www.example.com/foo/ and only one visit to the other
  //   URLs.
  // Other results:
  //   https://example.com/foo/ has the highest possible prefix rank, and it
  //   does not dupe the autofill result, so only it should be included.
  let context = createContext("example.com/foo/", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/foo/",
    completed: "https://www.example.com/foo/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://www.example.com/foo/",
        title: "Example Page",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://example.com/foo/",
        title: "Example Page",
      }),
    ],
  });

  // Add more visits to the lowest-priority prefix. It should be the heuristic
  // result but we should still show our highest-priority result. https://www.
  // should not appear at all.
  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://www.example.com/foo/",
        title: "Example Page",
      },
    ]);
  }

  // Expected results:
  //
  // Autofill result:
  //   http://www.example.com now has the highest origin frecency since we added
  //   4 visits to http://www.example.com/foo/
  // Other results:
  //   Same as before
  context = createContext("example.com/foo/", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/foo/",
    completed: "http://www.example.com/foo/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "http://www.example.com/foo/",
        title: "Example Page",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://example.com/foo/",
        title: "Example Page",
      }),
    ],
  });

  // Add enough https:// vists for it to have the highest frecency.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([
      {
        uri: "https://example.com/foo/",
        title: "Example Page",
      },
    ]);
  }

  // Expected results:
  //
  // Autofill result:
  //   https://example.com now has the highest origin frecency since we added
  //   6 visits to https://example.com/foo/
  // Other results:
  //   https://example.com/foo/ has the highest possible prefix rank. It dupes
  //   the heuristic so ordinarily it should not be included, but because the
  //   heuristic is hidden, only it should appear.
  context = createContext("example.com/foo/", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/foo/",
    completed: "https://example.com/foo/",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(context, {
        uri: "https://example.com/foo/",
        title: "Example Page",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "https://example.com/foo/",
        title: "Example Page",
      }),
    ],
  });

  await cleanupPlaces();
  UrlbarPrefs.clear("experimental.hideHeuristic");
});
