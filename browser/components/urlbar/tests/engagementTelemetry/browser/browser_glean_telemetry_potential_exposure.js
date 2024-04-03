/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the `urlbar-potential-exposure` ping.

const WAIT_FOR_PING_TIMEOUT_MS = 1000;

// Avoid timeouts in verify mode, especially on Mac.
requestLongerTimeout(3);

add_setup(async function test_setup() {
  Services.fog.testResetFOG();

  // Add a mock engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  registerCleanupFunction(() => {
    Services.fog.testResetFOG();
  });
});

add_task(async function oneKeyword_noMatch_1() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["exam"],
    expectedEvents: [],
  });
});

add_task(async function oneKeyword_noMatch_2() {
  await doTest({
    keywords: ["exam"],
    searchStrings: ["example"],
    expectedEvents: [],
  });
});

add_task(async function oneKeyword_oneMatch_terminal_1() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["example"],
    expectedEvents: [{ extra: { keyword: "example", terminal: true } }],
  });
});

add_task(async function oneKeyword_oneMatch_terminal_2() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["exam", "example"],
    expectedEvents: [{ extra: { keyword: "example", terminal: true } }],
  });
});

add_task(async function oneKeyword_oneMatch_nonterminal_1() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["example", "exam"],
    expectedEvents: [{ extra: { keyword: "example", terminal: false } }],
  });
});

add_task(async function oneKeyword_oneMatch_nonterminal_2() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["ex", "example", "exam"],
    expectedEvents: [{ extra: { keyword: "example", terminal: false } }],
  });
});

add_task(async function oneKeyword_dupeMatches_terminal_1() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["example", "example"],
    expectedEvents: [{ extra: { keyword: "example", terminal: true } }],
  });
});

add_task(async function oneKeyword_dupeMatches_terminal_2() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["example", "exampl", "example"],
    expectedEvents: [{ extra: { keyword: "example", terminal: true } }],
  });
});

add_task(async function oneKeyword_dupeMatches_terminal_3() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["exam", "example", "example"],
    expectedEvents: [{ extra: { keyword: "example", terminal: true } }],
  });
});

add_task(async function oneKeyword_dupeMatches_terminal_4() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["exam", "example", "exampl", "example"],
    expectedEvents: [{ extra: { keyword: "example", terminal: true } }],
  });
});

add_task(async function oneKeyword_dupeMatches_nonterminal_1() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["example", "example", "exampl"],
    expectedEvents: [{ extra: { keyword: "example", terminal: false } }],
  });
});

add_task(async function oneKeyword_dupeMatches_nonterminal_2() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["exam", "example", "example", "exampl"],
    expectedEvents: [{ extra: { keyword: "example", terminal: false } }],
  });
});

add_task(async function oneKeyword_dupeMatches_nonterminal_3() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["example", "exam", "example", "exampl"],
    expectedEvents: [{ extra: { keyword: "example", terminal: false } }],
  });
});

add_task(async function oneKeyword_dupeMatches_nonterminal_4() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["exam", "example", "exampl", "example", "exampl"],
    expectedEvents: [{ extra: { keyword: "example", terminal: false } }],
  });
});

add_task(async function manyKeywords_noMatch() {
  await doTest({
    keywords: ["foo", "bar", "baz"],
    searchStrings: ["example"],
    expectedEvents: [],
  });
});

add_task(async function manyKeywords_oneMatch_terminal_1() {
  await doTest({
    keywords: ["foo", "bar", "baz"],
    searchStrings: ["bar"],
    expectedEvents: [{ extra: { keyword: "bar", terminal: true } }],
  });
});

add_task(async function manyKeywords_oneMatch_terminal_2() {
  await doTest({
    keywords: ["foo", "bar", "baz"],
    searchStrings: ["example", "bar"],
    expectedEvents: [{ extra: { keyword: "bar", terminal: true } }],
  });
});

add_task(async function manyKeywords_oneMatch_nonterminal_1() {
  await doTest({
    keywords: ["foo", "bar", "baz"],
    searchStrings: ["bar", "example"],
    expectedEvents: [{ extra: { keyword: "bar", terminal: false } }],
  });
});

add_task(async function manyKeywords_oneMatch_nonterminal_2() {
  await doTest({
    keywords: ["foo", "bar", "baz"],
    searchStrings: ["exam", "bar", "example"],
    expectedEvents: [{ extra: { keyword: "bar", terminal: false } }],
  });
});

add_task(async function manyKeywords_manyMatches_terminal_1() {
  let keywords = ["foo", "bar", "baz"];
  await doTest({
    keywords,
    searchStrings: keywords,
    expectedEvents: keywords.map((keyword, i) => ({
      extra: { keyword, terminal: i == keywords.length - 1 },
    })),
  });
});

add_task(async function manyKeywords_manyMatches_terminal_2() {
  let keywords = ["foo", "bar", "baz"];
  await doTest({
    keywords,
    searchStrings: ["exam", "foo", "exampl", "bar", "example", "baz"],
    expectedEvents: keywords.map((keyword, i) => ({
      extra: { keyword, terminal: i == keywords.length - 1 },
    })),
  });
});

add_task(async function manyKeywords_manyMatches_nonterminal_1() {
  let keywords = ["foo", "bar", "baz"];
  await doTest({
    keywords,
    searchStrings: ["foo", "bar", "baz", "example"],
    expectedEvents: keywords.map(keyword => ({
      extra: { keyword, terminal: false },
    })),
  });
});

add_task(async function manyKeywords_manyMatches_nonterminal_2() {
  let keywords = ["foo", "bar", "baz"];
  await doTest({
    keywords,
    searchStrings: ["exam", "foo", "exampl", "bar", "example", "baz", "exam"],
    expectedEvents: keywords.map(keyword => ({
      extra: { keyword, terminal: false },
    })),
  });
});

add_task(async function manyKeywords_dupeMatches_terminal() {
  let keywords = ["foo", "bar", "baz"];
  let searchStrings = [...keywords, ...keywords];
  await doTest({
    keywords,
    searchStrings,
    expectedEvents: keywords.map((keyword, i) => ({
      extra: { keyword, terminal: i == keywords.length - 1 },
    })),
  });
});

add_task(async function manyKeywords_dupeMatches_nonterminal() {
  let keywords = ["foo", "bar", "baz"];
  let searchStrings = [...keywords, ...keywords, "example"];
  await doTest({
    keywords,
    searchStrings,
    expectedEvents: keywords.map(keyword => ({
      extra: { keyword, terminal: false },
    })),
  });
});

add_task(async function searchStringNormalization_terminal() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["    ExaMPLe  "],
    expectedEvents: [{ extra: { keyword: "example", terminal: true } }],
  });
});

add_task(async function searchStringNormalization_nonterminal() {
  await doTest({
    keywords: ["example"],
    searchStrings: ["    ExaMPLe  ", "foo"],
    expectedEvents: [{ extra: { keyword: "example", terminal: false } }],
  });
});

add_task(async function multiWordKeyword() {
  await doTest({
    keywords: ["this has multiple words"],
    searchStrings: ["this has multiple words"],
    expectedEvents: [
      { extra: { keyword: "this has multiple words", terminal: true } },
    ],
  });
});

// Smoke test that ends a session with an engagement instead of an abandonment
// as other tasks in this file do.
add_task(async function engagement() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await doTest({
      keywords: ["example"],
      searchStrings: ["example"],
      endSession: () =>
        // Hit the Enter key on the heuristic search result.
        UrlbarTestUtils.promisePopupClose(window, () =>
          EventUtils.synthesizeKey("KEY_Enter")
        ),
      expectedEvents: [{ extra: { keyword: "example", terminal: true } }],
    });
  });
});

// Smoke test that uses Nimbus to set keywords instead of a pref as other tasks
// in this file do.
add_task(async function nimbus() {
  let keywords = ["foo", "bar", "baz"];
  await doTest({
    useNimbus: true,
    keywords,
    searchStrings: keywords,
    expectedEvents: keywords.map((keyword, i) => ({
      extra: { keyword, terminal: i == keywords.length - 1 },
    })),
  });
});

// The ping should not be submitted for sessions in private windows.
add_task(async function privateWindow() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await doTest({
    win: privateWin,
    keywords: ["example"],
    searchStrings: ["example"],
    expectedEvents: [],
  });
  await BrowserTestUtils.closeWindow(privateWin);
});

add_task(async function invalidPotentialExposureKeywords_pref() {
  await doTest({
    keywords: "not an array of keywords",
    searchStrings: ["example", "not an array of keywords"],
    expectedEvents: [],
  });
});

add_task(async function invalidPotentialExposureKeywords_nimbus() {
  await doTest({
    useNimbus: true,
    keywords: "not an array of keywords",
    searchStrings: ["example", "not an array of keywords"],
    expectedEvents: [],
  });
});

async function doTest({
  keywords,
  searchStrings,
  expectedEvents,
  endSession = null,
  useNimbus = false,
  win = window,
}) {
  endSession ||= () =>
    UrlbarTestUtils.promisePopupClose(win, () => win.gURLBar.blur());

  let nimbusCleanup;
  let keywordsJson = JSON.stringify(keywords);
  if (useNimbus) {
    nimbusCleanup = await UrlbarTestUtils.initNimbusFeature({
      potentialExposureKeywords: keywordsJson,
    });
  } else {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.potentialExposureKeywords", keywordsJson]],
    });
  }

  let pingPromise = waitForPing();

  for (let value of searchStrings) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      value,
      window: win,
    });
  }
  await endSession();

  // Wait `WAIT_FOR_PING_TIMEOUT_MS` for the ping to be submitted before
  // reporting a timeout. Note that some tasks do not expect a ping to be
  // submitted, and they rely on this timeout behavior.
  info("Awaiting ping promise");
  let events = null;
  events = await Promise.race([
    pingPromise,
    new Promise(resolve =>
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(() => {
        if (!events) {
          info("Timed out waiting for ping");
        }
        resolve([]);
      }, WAIT_FOR_PING_TIMEOUT_MS)
    ),
  ]);

  assertEvents(events, expectedEvents);

  if (nimbusCleanup) {
    await nimbusCleanup();
  } else {
    await SpecialPowers.popPrefEnv();
  }
  Services.fog.testResetFOG();
}

function waitForPing() {
  return new Promise(resolve => {
    GleanPings.urlbarPotentialExposure.testBeforeNextSubmit(() => {
      let events = Glean.urlbar.potentialExposure.testGetValue();
      info("testBeforeNextSubmit got events: " + JSON.stringify(events));
      resolve(events);
    });
  });
}

function assertEvents(actual, expected) {
  info("Comparing events: " + JSON.stringify({ actual, expected }));

  // Add some expected boilerplate properties to the expected events so that
  // callers don't have to but so that we still check them.
  expected = expected.map(e => ({
    category: "urlbar",
    name: "potential_exposure",
    // `testGetValue()` stringifies booleans for some reason. Let callers
    // specify booleans since booleans are correct, and stringify them here.
    ...stringifyBooleans(e),
  }));

  // Filter out properties from the actual events that aren't defined in the
  // expected events. Ignore unimportant properties like timestamps.
  actual = actual.map((a, i) =>
    Object.fromEntries(
      Object.entries(a).filter(([key]) => expected[i]?.hasOwnProperty(key))
    )
  );

  Assert.deepEqual(actual, expected, "Checking expected Glean events");
}

function stringifyBooleans(obj) {
  let newObj = {};
  for (let [key, value] of Object.entries(obj)) {
    if (value && typeof value == "object") {
      newObj[key] = stringifyBooleans(value);
    } else if (typeof value == "boolean") {
      newObj[key] = String(value);
    } else {
      newObj[key] = value;
    }
  }
  return newObj;
}
