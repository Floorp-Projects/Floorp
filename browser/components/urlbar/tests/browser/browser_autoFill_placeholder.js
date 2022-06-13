/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that the autofill placeholder value is autofilled
// correctly.  The placeholder is a string that we immediately autofill when a
// search starts and before its first result arrives in order to prevent text
// flicker in the input.
//
// Because this test specifically checks autofill *before* searches complete, we
// can't use promiseAutocompleteResultPopup() or other helpers that wait for
// searches to complete.  Instead the test uses fireInputEvent() to trigger
// placeholder autofill and then immediately checks autofill status.

"use strict";

// Allow more time for verify mode.
requestLongerTimeout(5);

add_task(async function init() {
  await cleanUp();
});

// Basic origin autofill test.
add_task(async function origin() {
  await addVisits("http://example.com/");

  await search({
    searchString: "ex",
    valueBefore: "ex",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });
  await search({
    searchString: "exa",
    valueBefore: "example.com/",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });
  await search({
    searchString: "EXAM",
    valueBefore: "EXAMple.com/",
    valueAfter: "EXAMple.com/",
    placeholderAfter: "EXAMple.com/",
  });
  await search({
    searchString: "eXaMp",
    valueBefore: "eXaMple.com/",
    valueAfter: "eXaMple.com/",
    placeholderAfter: "eXaMple.com/",
  });
  await search({
    searchString: "exampL",
    valueBefore: "exampLe.com/",
    valueAfter: "exampLe.com/",
    placeholderAfter: "exampLe.com/",
  });
  await search({
    searchString: "example.com",
    valueBefore: "example.com/",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });
  await search({
    searchString: "example.com/",
    valueBefore: "example.com/",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });

  await cleanUp();
});

// Basic URL autofill test.
add_task(async function url() {
  await addVisits("http://example.com/aaa/bbb/ccc");

  await search({
    searchString: "example.com/a",
    valueBefore: "example.com/a",
    valueAfter: "example.com/aaa/",
    placeholderAfter: "example.com/aaa/",
  });
  await search({
    searchString: "EXAmple.com/aA",
    valueBefore: "EXAmple.com/aAa/",
    valueAfter: "EXAmple.com/aAa/",
    placeholderAfter: "EXAmple.com/aAa/",
  });
  await search({
    searchString: "example.com/aAa",
    valueBefore: "example.com/aAa/",
    valueAfter: "example.com/aAa/",
    placeholderAfter: "example.com/aAa/",
  });
  await search({
    searchString: "example.com/aaa/",
    valueBefore: "example.com/aaa/",
    valueAfter: "example.com/aaa/",
    placeholderAfter: "example.com/aaa/",
  });
  await search({
    searchString: "example.com/aaa/b",
    valueBefore: "example.com/aaa/b",
    valueAfter: "example.com/aaa/bbb/",
    placeholderAfter: "example.com/aaa/bbb/",
  });
  await search({
    searchString: "example.com/aAa/bB",
    valueBefore: "example.com/aAa/bBb/",
    valueAfter: "example.com/aAa/bBb/",
    placeholderAfter: "example.com/aAa/bBb/",
  });
  await search({
    searchString: "example.com/aAa/bBb",
    valueBefore: "example.com/aAa/bBb/",
    valueAfter: "example.com/aAa/bBb/",
    placeholderAfter: "example.com/aAa/bBb/",
  });
  await search({
    searchString: "example.com/aaa/bbb/",
    valueBefore: "example.com/aaa/bbb/",
    valueAfter: "example.com/aaa/bbb/",
    placeholderAfter: "example.com/aaa/bbb/",
  });
  await search({
    searchString: "example.com/aaa/bbb/c",
    valueBefore: "example.com/aaa/bbb/c",
    valueAfter: "example.com/aaa/bbb/ccc",
    placeholderAfter: "example.com/aaa/bbb/ccc",
  });
  await search({
    searchString: "example.com/aAa/bBb/cC",
    valueBefore: "example.com/aAa/bBb/cCc",
    valueAfter: "example.com/aAa/bBb/cCc",
    placeholderAfter: "example.com/aAa/bBb/cCc",
  });
  await search({
    searchString: "example.com/aaa/bbb/ccc",
    valueBefore: "example.com/aaa/bbb/ccc",
    valueAfter: "example.com/aaa/bbb/ccc",
    placeholderAfter: "example.com/aaa/bbb/ccc",
  });

  await cleanUp();
});

// Basic adaptive history autofill test.
add_task(async function adaptiveHistory() {
  UrlbarPrefs.set("autoFill.adaptiveHistory.enabled", true);

  await addVisits("http://example.com/test");
  await UrlbarUtils.addToInputHistory("http://example.com/test", "exa");

  await search({
    searchString: "exa",
    valueBefore: "exa",
    valueAfter: "example.com/test",
    placeholderAfter: "example.com/test",
  });
  await search({
    searchString: "EXAM",
    valueBefore: "EXAMple.com/test",
    valueAfter: "EXAMple.com/test",
    placeholderAfter: "EXAMple.com/test",
  });
  await search({
    searchString: "eXaMpLe",
    valueBefore: "eXaMpLe.com/test",
    valueAfter: "eXaMpLe.com/test",
    placeholderAfter: "eXaMpLe.com/test",
  });
  await search({
    searchString: "example.",
    valueBefore: "example.com/test",
    valueAfter: "example.com/test",
    placeholderAfter: "example.com/test",
  });
  await search({
    searchString: "example.c",
    valueBefore: "example.com/test",
    valueAfter: "example.com/test",
    placeholderAfter: "example.com/test",
  });
  await search({
    searchString: "example.com",
    valueBefore: "example.com/test",
    valueAfter: "example.com/test",
    placeholderAfter: "example.com/test",
  });
  await search({
    searchString: "example.com/",
    valueBefore: "example.com/test",
    valueAfter: "example.com/test",
    placeholderAfter: "example.com/test",
  });
  await search({
    searchString: "example.com/T",
    valueBefore: "example.com/Test",
    valueAfter: "example.com/Test",
    placeholderAfter: "example.com/Test",
  });
  await search({
    searchString: "eXaMple.com/tE",
    valueBefore: "eXaMple.com/tEst",
    valueAfter: "eXaMple.com/tEst",
    placeholderAfter: "eXaMple.com/tEst",
  });
  await search({
    searchString: "example.com/tes",
    valueBefore: "example.com/test",
    valueAfter: "example.com/test",
    placeholderAfter: "example.com/test",
  });
  await search({
    searchString: "example.com/test",
    valueBefore: "example.com/test",
    valueAfter: "example.com/test",
    placeholderAfter: "example.com/test",
  });

  UrlbarPrefs.clear("autoFill.adaptiveHistory.enabled");
  await cleanUp();
});

// Search engine token alias test (aliases that start with "@").
add_task(async function tokenAlias() {
  // We have built-in engine aliases that may conflict with the one we choose
  // here in terms of autofill, so be careful and choose a weird alias.
  await SearchTestUtils.installSearchExtension({ keyword: "@__example" });

  await search({
    searchString: "@__ex",
    valueBefore: "@__ex",
    valueAfter: "@__example ",
    placeholderAfter: "@__example ",
  });
  await search({
    searchString: "@__exa",
    valueBefore: "@__example ",
    valueAfter: "@__example ",
    placeholderAfter: "@__example ",
  });
  await search({
    searchString: "@__EXAM",
    valueBefore: "@__EXAMple ",
    valueAfter: "@__EXAMple ",
    placeholderAfter: "@__EXAMple ",
  });
  await search({
    searchString: "@__eXaMp",
    valueBefore: "@__eXaMple ",
    valueAfter: "@__eXaMple ",
    placeholderAfter: "@__eXaMple ",
  });
  await search({
    searchString: "@__exampl",
    valueBefore: "@__example ",
    valueAfter: "@__example ",
    placeholderAfter: "@__example ",
  });

  await cleanUp();
});

// The placeholder should not be used for a search that does not autofill, and
// it should be cleared after the search completes.
add_task(async function noAutofill() {
  await addVisits("http://example.com/");

  // Do an initial search that triggers autofill so that the placeholder has an
  // initial value.
  await search({
    searchString: "ex",
    valueBefore: "ex",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });

  // Search with a string that does not match the placeholder.  Placeholder
  // autofill shouldn't happen.
  await search({
    searchString: "moz",
    valueBefore: "moz",
    valueAfter: "moz",
    placeholderAfter: "",
  });

  // Search for "ex" again. It should be autofilled when the search completes
  // but the placeholder will not be autofilled.
  await search({
    searchString: "ex",
    valueBefore: "ex",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });

  // Continue with a series of searches that should all use the placeholder.
  await search({
    searchString: "exa",
    valueBefore: "example.com/",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });
  await search({
    searchString: "EXAM",
    valueBefore: "EXAMple.com/",
    valueAfter: "EXAMple.com/",
    placeholderAfter: "EXAMple.com/",
  });
  await search({
    searchString: "eXaMp",
    valueBefore: "eXaMple.com/",
    valueAfter: "eXaMple.com/",
    placeholderAfter: "eXaMple.com/",
  });
  await search({
    searchString: "exampl",
    valueBefore: "example.com/",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });

  await cleanUp();
});

// The placeholder should not be used for a search that autofills a different
// value.
add_task(async function differentAutofill() {
  await addVisits("http://mozilla.org/", "http://example.com/");

  // Do an initial search that triggers autofill so that the placeholder has an
  // initial value.
  await search({
    searchString: "moz",
    valueBefore: "moz",
    valueAfter: "mozilla.org/",
    placeholderAfter: "mozilla.org/",
  });

  // Search with a string that does not match the placeholder but does trigger
  // autofill.  Placeholder autofill shouldn't happen.
  await search({
    searchString: "ex",
    valueBefore: "ex",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });

  // Continue with a series of searches that should all use the placeholder.
  await search({
    searchString: "exa",
    valueBefore: "example.com/",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });
  await search({
    searchString: "EXAm",
    valueBefore: "EXAmple.com/",
    valueAfter: "EXAmple.com/",
    placeholderAfter: "EXAmple.com/",
  });

  // Search for "moz" again.  It should be autofilled.  Placeholder autofill
  // shouldn't happen.
  await search({
    searchString: "moz",
    valueBefore: "moz",
    valueAfter: "mozilla.org/",
    placeholderAfter: "mozilla.org/",
  });

  // Continue with a series of searches that should all use the placeholder.
  await search({
    searchString: "mozi",
    valueBefore: "mozilla.org/",
    valueAfter: "mozilla.org/",
    placeholderAfter: "mozilla.org/",
  });
  await search({
    searchString: "MOZil",
    valueBefore: "MOZilla.org/",
    valueAfter: "MOZilla.org/",
    placeholderAfter: "MOZilla.org/",
  });

  await cleanUp();
});

// The placeholder should not be used for a search that uses a bookmark keyword
// even when the keyword matches the placeholder, and the placeholder should be
// cleared after the search completes.
add_task(async function bookmarkKeyword() {
  // Add a visit to example.com.
  await addVisits("https://example.com/");

  // Add a bookmark keyword that is a prefix of example.com.
  await PlacesUtils.keywords.insert({
    keyword: "ex",
    url: "https://somekeyword.com/",
  });

  // Do an initial search that triggers autofill for the visit so that the
  // placeholder has an initial value of "example.com/".
  await search({
    searchString: "e",
    valueBefore: "e",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });

  // Do a search that matches the bookmark keyword. The placeholder from the
  // search above should be autofilled since the autofill placeholder
  // ("example.com/") starts with the keyword ("ex"), but then when the bookmark
  // result arrives, the autofilled value and placeholder should be cleared.
  await search({
    searchString: "ex",
    valueBefore: "example.com/",
    valueAfter: "ex",
    placeholderAfter: "",
  });

  // Do another search that simulates the user continuing to type "example". No
  // placeholder should be autofilled, but once the autofill result arrives for
  // the visit, "example.com/" should be autofilled.
  await search({
    searchString: "exa",
    valueBefore: "exa",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });

  await PlacesUtils.keywords.remove("ex");
  await cleanUp();
});

// The placeholder should not be used for a search that doesn't match its URI
// fragment. This task uses a URL whose path is "/".
add_task(async function noURIFragmentMatch1() {
  await addVisits("https://example.com/#TEST");

  const testData = [
    {
      desc: "Autofill example.com/#TEST then search for example.com/#Te",
      searches: [
        {
          searchString: "example.com/#T",
          valueBefore: "example.com/#T",
          valueAfter: "example.com/#TEST",
          placeholderAfter: "example.com/#TEST",
        },
        {
          searchString: "example.com/#Te",
          valueBefore: "example.com/#Te",
          valueAfter: "example.com/#Te",
          placeholderAfter: "",
        },
      ],
    },
    {
      desc:
        "Autofill https://example.com/#TEST then search for https://example.com/#Te",
      searches: [
        {
          searchString: "https://example.com/#T",
          valueBefore: "https://example.com/#T",
          valueAfter: "https://example.com/#TEST",
          placeholderAfter: "https://example.com/#TEST",
        },
        {
          searchString: "https://example.com/#Te",
          valueBefore: "https://example.com/#Te",
          valueAfter: "https://example.com/#Te",
          placeholderAfter: "",
        },
      ],
    },
    {
      desc: "Autofill example.com/#TEST then search for example.com/",
      searches: [
        {
          searchString: "example.com/#T",
          valueBefore: "example.com/#T",
          valueAfter: "example.com/#TEST",
          placeholderAfter: "example.com/#TEST",
        },
        {
          searchString: "example.com/",
          valueBefore: "example.com/",
          valueAfter: "example.com/",
          placeholderAfter: "example.com/",
        },
      ],
    },
  ];

  for (const { desc, searches } of testData) {
    info("Running subtest: " + desc);

    for (let i = 0; i < searches.length; i++) {
      info("Doing search at index " + i);
      await search(searches[i]);
    }

    // Clear the placeholder for the next subtest.
    info("Doing extra search to clear placeholder");
    await search({
      searchString: "no match",
      valueBefore: "no match",
      valueAfter: "no match",
      placeholderAfter: "",
    });
  }

  await cleanUp();
});

// The placeholder should not be used for a search that doesn't match its URI
// fragment. This task uses a URL whose path is "/foo".
add_task(async function noURIFragmentMatch2() {
  await addVisits("https://example.com/foo#TEST");

  const testData = [
    {
      desc: "Autofill example.com/foo#TEST then search for example.com/foo#Te",
      searches: [
        {
          searchString: "example.com/foo#T",
          valueBefore: "example.com/foo#T",
          valueAfter: "example.com/foo#TEST",
          placeholderAfter: "example.com/foo#TEST",
        },
        {
          searchString: "example.com/foo#Te",
          valueBefore: "example.com/foo#Te",
          valueAfter: "example.com/foo#Te",
          placeholderAfter: "",
        },
      ],
    },
    {
      desc:
        "Autofill https://example.com/foo#TEST then search for https://example.com/foo#Te",
      searches: [
        {
          searchString: "https://example.com/foo#T",
          valueBefore: "https://example.com/foo#T",
          valueAfter: "https://example.com/foo#TEST",
          placeholderAfter: "https://example.com/foo#TEST",
        },
        {
          searchString: "https://example.com/foo#Te",
          valueBefore: "https://example.com/foo#Te",
          valueAfter: "https://example.com/foo#Te",
          placeholderAfter: "",
        },
      ],
    },
    {
      desc: "Autofill example.com/foo#TEST then search for example.com/",
      searches: [
        {
          searchString: "example.com/foo#T",
          valueBefore: "example.com/foo#T",
          valueAfter: "example.com/foo#TEST",
          placeholderAfter: "example.com/foo#TEST",
        },
        {
          searchString: "example.com/",
          valueBefore: "example.com/",
          valueAfter: "example.com/",
          placeholderAfter: "example.com/",
        },
      ],
    },
  ];

  for (const { desc, searches } of testData) {
    info("Running subtest: " + desc);

    for (let i = 0; i < searches.length; i++) {
      info("Doing search at index " + i);
      await search(searches[i]);
    }

    // Clear the placeholder for the next subtest.
    info("Doing extra search to clear placeholder");
    await search({
      searchString: "no match",
      valueBefore: "no match",
      valueAfter: "no match",
      placeholderAfter: "",
    });
  }

  await cleanUp();
});

// The placeholder should not be used for a search that does not autofill its
// URL path.
add_task(async function noPathMatch() {
  await addVisits("http://example.com/shallow/deep/file");

  const testData = [
    {
      desc: "Autofill example.com/shallow/ then search for exam",
      searches: [
        {
          searchString: "example.com/s",
          valueBefore: "example.com/s",
          valueAfter: "example.com/shallow/",
          placeholderAfter: "example.com/shallow/",
        },
        {
          searchString: "exam",
          valueBefore: "exam",
          valueAfter: "example.com/",
          placeholderAfter: "example.com/",
        },
      ],
    },
    {
      desc: "Autofill example.com/shallow/ then search for example.com/",
      searches: [
        {
          searchString: "example.com/s",
          valueBefore: "example.com/s",
          valueAfter: "example.com/shallow/",
          placeholderAfter: "example.com/shallow/",
        },
        {
          searchString: "example.com/",
          valueBefore: "example.com/",
          valueAfter: "example.com/",
          placeholderAfter: "example.com/",
        },
      ],
    },
    {
      desc: "Autofill example.com/shallow/deep/ then search for exam",
      searches: [
        {
          searchString: "example.com/shallow/d",
          valueBefore: "example.com/shallow/d",
          valueAfter: "example.com/shallow/deep/",
          placeholderAfter: "example.com/shallow/deep/",
        },
        {
          searchString: "exam",
          valueBefore: "exam",
          valueAfter: "example.com/",
          placeholderAfter: "example.com/",
        },
      ],
    },
    {
      desc: "Autofill example.com/shallow/deep/ then search for example.com/",
      searches: [
        {
          searchString: "example.com/shallow/d",
          valueBefore: "example.com/shallow/d",
          valueAfter: "example.com/shallow/deep/",
          placeholderAfter: "example.com/shallow/deep/",
        },
        {
          searchString: "example.com/",
          valueBefore: "example.com/",
          valueAfter: "example.com/",
          placeholderAfter: "example.com/",
        },
      ],
    },
    {
      desc: "Autofill example.com/shallow/deep/ then search for example.com/s",
      searches: [
        {
          searchString: "example.com/shallow/d",
          valueBefore: "example.com/shallow/d",
          valueAfter: "example.com/shallow/deep/",
          placeholderAfter: "example.com/shallow/deep/",
        },
        {
          searchString: "example.com/s",
          valueBefore: "example.com/s",
          valueAfter: "example.com/shallow/",
          placeholderAfter: "example.com/shallow/",
        },
      ],
    },
    {
      desc:
        "Autofill example.com/shallow/deep/ then search for example.com/shallow/",
      searches: [
        {
          searchString: "example.com/shallow/d",
          valueBefore: "example.com/shallow/d",
          valueAfter: "example.com/shallow/deep/",
          placeholderAfter: "example.com/shallow/deep/",
        },
        {
          searchString: "example.com/shallow/",
          valueBefore: "example.com/shallow/",
          valueAfter: "example.com/shallow/",
          placeholderAfter: "example.com/shallow/",
        },
      ],
    },
    {
      desc: "Autofill example.com/shallow/deep/file then search for exam",
      searches: [
        {
          searchString: "example.com/shallow/deep/f",
          valueBefore: "example.com/shallow/deep/f",
          valueAfter: "example.com/shallow/deep/file",
          placeholderAfter: "example.com/shallow/deep/file",
        },
        {
          searchString: "exam",
          valueBefore: "exam",
          valueAfter: "example.com/",
          placeholderAfter: "example.com/",
        },
      ],
    },
    {
      desc:
        "Autofill example.com/shallow/deep/file then search for example.com/",
      searches: [
        {
          searchString: "example.com/shallow/deep/f",
          valueBefore: "example.com/shallow/deep/f",
          valueAfter: "example.com/shallow/deep/file",
          placeholderAfter: "example.com/shallow/deep/file",
        },
        {
          searchString: "example.com/",
          valueBefore: "example.com/",
          valueAfter: "example.com/",
          placeholderAfter: "example.com/",
        },
      ],
    },
    {
      desc:
        "Autofill example.com/shallow/deep/file then search for example.com/s",
      searches: [
        {
          searchString: "example.com/shallow/deep/f",
          valueBefore: "example.com/shallow/deep/f",
          valueAfter: "example.com/shallow/deep/file",
          placeholderAfter: "example.com/shallow/deep/file",
        },
        {
          searchString: "example.com/s",
          valueBefore: "example.com/s",
          valueAfter: "example.com/shallow/",
          placeholderAfter: "example.com/shallow/",
        },
      ],
    },
    {
      desc:
        "Autofill example.com/shallow/deep/file then search for example.com/shallow/",
      searches: [
        {
          searchString: "example.com/shallow/deep/f",
          valueBefore: "example.com/shallow/deep/f",
          valueAfter: "example.com/shallow/deep/file",
          placeholderAfter: "example.com/shallow/deep/file",
        },
        {
          searchString: "example.com/shallow/",
          valueBefore: "example.com/shallow/",
          valueAfter: "example.com/shallow/",
          placeholderAfter: "example.com/shallow/",
        },
      ],
    },
    {
      desc:
        "Autofill example.com/shallow/deep/file then search for example.com/shallow/d",
      searches: [
        {
          searchString: "example.com/shallow/deep/f",
          valueBefore: "example.com/shallow/deep/f",
          valueAfter: "example.com/shallow/deep/file",
          placeholderAfter: "example.com/shallow/deep/file",
        },
        {
          searchString: "example.com/shallow/d",
          valueBefore: "example.com/shallow/d",
          valueAfter: "example.com/shallow/deep/",
          placeholderAfter: "example.com/shallow/deep/",
        },
      ],
    },
    {
      desc:
        "Autofill example.com/shallow/deep/file then search for example.com/shallow/deep/",
      searches: [
        {
          searchString: "example.com/shallow/deep/f",
          valueBefore: "example.com/shallow/deep/f",
          valueAfter: "example.com/shallow/deep/file",
          placeholderAfter: "example.com/shallow/deep/file",
        },
        {
          searchString: "example.com/shallow/deep/fi",
          valueBefore: "example.com/shallow/deep/file",
          valueAfter: "example.com/shallow/deep/file",
          placeholderAfter: "example.com/shallow/deep/file",
        },
        {
          searchString: "example.com/shallow/deep/",
          valueBefore: "example.com/shallow/deep/",
          valueAfter: "example.com/shallow/deep/",
          placeholderAfter: "example.com/shallow/deep/",
        },
      ],
    },
  ];

  for (const { desc, searches } of testData) {
    info("Running subtest: " + desc);

    for (let i = 0; i < searches.length; i++) {
      info("Doing search at index " + i);
      await search(searches[i]);
    }

    // Clear the placeholder for the next subtest.
    info("Doing extra search to clear placeholder");
    await search({
      searchString: "no match",
      valueBefore: "no match",
      valueAfter: "no match",
      placeholderAfter: "",
    });
  }

  await cleanUp();
});

// An adaptive history placeholder should not be used for a search that does not
// autofill it.
add_task(async function noAdaptiveHistoryMatch() {
  UrlbarPrefs.set("autoFill.adaptiveHistory.enabled", true);

  await addVisits("http://example.com/test");
  await UrlbarUtils.addToInputHistory("http://example.com/test", "exam");

  // Search for a longer string than the adaptive history input. Adaptive
  // history autofill should be triggered.
  await search({
    searchString: "example",
    valueBefore: "example",
    valueAfter: "example.com/test",
    placeholderAfter: "example.com/test",
  });

  // Search for the same string as the adaptive history input. The placeholder
  // from the previous search should be used and adaptive history autofill
  // should be triggered.
  await search({
    searchString: "exam",
    valueBefore: "example.com/test",
    valueAfter: "example.com/test",
    placeholderAfter: "example.com/test",
  });

  // Search for a shorter string than the adaptive history input. The
  // placeholder from the previous search should not be used since the search
  // string is shorter than the adaptive history input.
  await search({
    searchString: "ex",
    valueBefore: "ex",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });

  UrlbarPrefs.clear("autoFill.adaptiveHistory.enabled");
  await cleanUp();
});

/**
 * This function does the following:
 *
 * 1. Starts a search with `searchString` but doesn't wait for it to complete.
 * 2. Compares the input value to `valueBefore`. If anything is autofilled at
 *    this point, it will be due to the placeholder.
 * 3. Waits for the search to complete.
 * 4. Compares the input value to `valueAfter`. If anything is autofilled at
 *    this point, it will be due to the autofill result fetched by the search.
 * 5. Compares the placeholder to `placeholderAfter`.
 *
 * @param {string} searchString
 * @param {string} valueBefore
 *   The expected input value before the search completes.
 * @param {string} valueAfter
 *   The expected input value after the search completes.
 * @param {string} placeholderAfter
 *   The expected placeholder value after the search completes.
 */
async function search({
  searchString,
  valueBefore,
  valueAfter,
  placeholderAfter,
}) {
  info(
    "Searching: " +
      JSON.stringify({
        searchString,
        valueBefore,
        valueAfter,
        placeholderAfter,
      })
  );

  await SimpleTest.promiseFocus(window);
  gURLBar.inputField.focus();

  // Set the input value and move the caret to the end to simulate the user
  // typing. It's important the caret is at the end because otherwise autofill
  // won't happen.
  gURLBar.value = searchString;
  gURLBar.inputField.setSelectionRange(
    searchString.length,
    searchString.length
  );

  // Placeholder autofill is done on input, so fire an input event. We can't use
  // `promiseAutocompleteResultPopup()` or other helpers that wait for the
  // search to complete because we are specifically checking placeholder
  // autofill before the search completes.
  UrlbarTestUtils.fireInputEvent(window);

  // Check the input value and selection immediately, before waiting on the
  // search to complete.
  Assert.equal(
    gURLBar.value,
    valueBefore,
    "gURLBar.value before the search completes"
  );
  Assert.equal(
    gURLBar.selectionStart,
    searchString.length,
    "gURLBar.selectionStart before the search completes"
  );
  Assert.equal(
    gURLBar.selectionEnd,
    valueBefore.length,
    "gURLBar.selectionEnd before the search completes"
  );

  // Wait for the search to complete.
  info("Waiting for the search to complete");
  await UrlbarTestUtils.promiseSearchComplete(window);

  // Check the final value after the results arrived.
  Assert.equal(
    gURLBar.value,
    valueAfter,
    "gURLBar.value after the search completes"
  );
  Assert.equal(
    gURLBar.selectionStart,
    searchString.length,
    "gURLBar.selectionStart after the search completes"
  );
  Assert.equal(
    gURLBar.selectionEnd,
    valueAfter.length,
    "gURLBar.selectionEnd after the search completes"
  );

  // Check the placeholder.
  if (placeholderAfter) {
    Assert.ok(
      gURLBar._autofillPlaceholder,
      "gURLBar._autofillPlaceholder exists after the search completes"
    );
    Assert.strictEqual(
      gURLBar._autofillPlaceholder.value,
      placeholderAfter,
      "gURLBar._autofillPlaceholder.value after the search completes"
    );
  } else {
    Assert.strictEqual(
      gURLBar._autofillPlaceholder,
      null,
      "gURLBar._autofillPlaceholder does not exist after the search completes"
    );
  }

  // Check the first result.
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    !!details.autofill,
    !!placeholderAfter,
    "First result is an autofill result iff a placeholder is expected"
  );
}

/**
 * Adds enough visits to URLs so their origins start autofilling.
 *
 * @param {...string} urls
 */
async function addVisits(...urls) {
  for (let url of urls) {
    for (let i = 0; i < 5; i++) {
      await PlacesTestUtils.addVisits(url);
    }
  }
}

async function cleanUp() {
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
