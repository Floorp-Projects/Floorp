/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Interactions } = ChromeUtils.import(
  "resource:///modules/Interactions.jsm"
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "pageViewIdleTime",
  "browser.places.interactions.pageViewIdleTime",
  60
);

add_setup(async function global_setup() {
  // Disable idle management because it interacts with our code, causing
  // unexpected intermittent failures, we'll fake idle notifications when
  // we need to test it.
  let idleService = Cc["@mozilla.org/widget/useridleservice;1"].getService(
    Ci.nsIUserIdleService
  );
  idleService.removeIdleObserver(Interactions, pageViewIdleTime);
  registerCleanupFunction(() => {
    idleService.addIdleObserver(Interactions, pageViewIdleTime);
  });

  // Clean interactions for each test.
  await Interactions.reset();
  registerCleanupFunction(async () => {
    await Interactions.reset();
  });
});

/**
 * Ensures that a list of interactions have been permanently stored.
 * @param {Array} expected list of interactions to be found.
 * @param {boolean} [dontFlush] Avoid flushing pending data.
 */
async function assertDatabaseValues(expected, { dontFlush = false } = {}) {
  await Interactions.interactionUpdatePromise;
  if (!dontFlush) {
    await Interactions.store.flush();
  }

  let interactions = await PlacesUtils.withConnectionWrapper(
    "head.js::assertDatabaseValues",
    async db => {
      let rows = await db.execute(`
        SELECT h.url AS url, h2.url as referrer_url, total_view_time, key_presses, typing_time, scrolling_time, scrolling_distance
        FROM moz_places_metadata m
        JOIN moz_places h ON h.id = m.place_id
        LEFT JOIN moz_places h2 ON h2.id = m.referrer_place_id
        ORDER BY created_at ASC
      `);
      return rows.map(r => ({
        url: r.getResultByName("url"),
        referrerUrl: r.getResultByName("referrer_url"),
        keypresses: r.getResultByName("key_presses"),
        typingTime: r.getResultByName("typing_time"),
        totalViewTime: r.getResultByName("total_view_time"),
        scrollingTime: r.getResultByName("scrolling_time"),
        scrollingDistance: r.getResultByName("scrolling_distance"),
      }));
    }
  );
  info(
    `Found ${interactions.length} interactions:\n ${JSON.stringify(
      interactions
    )}`
  );
  Assert.equal(
    interactions.length,
    expected.length,
    "Found the expected number of entries"
  );
  for (let i = 0; i < Math.min(expected.length, interactions.length); i++) {
    let actual = interactions[i];
    Assert.equal(
      actual.url,
      expected[i].url,
      "Should have saved the page into the database"
    );

    if (expected[i].exactTotalViewTime != undefined) {
      Assert.equal(
        actual.totalViewTime,
        expected[i].exactTotalViewTime,
        "Should have kept the exact time."
      );
    } else if (expected[i].totalViewTime != undefined) {
      Assert.greaterOrEqual(
        actual.totalViewTime,
        expected[i].totalViewTime,
        "Should have stored the interaction time"
      );
    }

    if (expected[i].maxViewTime != undefined) {
      Assert.less(
        actual.totalViewTime,
        expected[i].maxViewTime,
        "Should have recorded an interaction below the maximum expected"
      );
    }

    if (expected[i].keypresses != undefined) {
      Assert.equal(
        actual.keypresses,
        expected[i].keypresses,
        "Should have saved the keypresses into the database"
      );
    }

    if (expected[i].exactTypingTime != undefined) {
      Assert.equal(
        actual.typingTime,
        expected[i].exactTypingTime,
        "Should have stored the exact typing time."
      );
    } else if (expected[i].typingTimeIsGreaterThan != undefined) {
      Assert.greater(
        actual.typingTime,
        expected[i].typingTimeIsGreaterThan,
        "Should have stored at least this amount of typing time."
      );
    } else if (expected[i].typingTimeIsLessThan != undefined) {
      Assert.less(
        actual.typingTime,
        expected[i].typingTimeIsLessThan,
        "Should have stored less than this amount of typing time."
      );
    }

    if (expected[i].exactScrollingDistance != undefined) {
      Assert.equal(
        actual.scrollingDistance,
        expected[i].exactScrollingDistance,
        "Should have scrolled by exactly least this distance"
      );
    } else if (expected[i].exactScrollingTime != undefined) {
      Assert.greater(
        actual.scrollingTime,
        expected[i].exactScrollingTime,
        "Should have scrolled for exactly least this duration"
      );
    }

    if (expected[i].scrollingDistanceIsGreaterThan != undefined) {
      Assert.greater(
        actual.scrollingDistance,
        expected[i].scrollingDistanceIsGreaterThan,
        "Should have scrolled by at least this distance"
      );
    } else if (expected[i].scrollingTimeIsGreaterThan != undefined) {
      Assert.greater(
        actual.scrollingTime,
        expected[i].scrollingTimeIsGreaterThan,
        "Should have scrolled for at least this duration"
      );
    }
  }
}

/**
 * Ensures that a list of interactions have been permanently stored.
 * @param {string} url The url to query.
 * @param {string} property The property to extract.
 */
async function getDatabaseValue(url, property) {
  await Interactions.store.flush();
  const PROP_TRANSLATOR = {
    totalViewTime: "total_view_time",
    keypresses: "key_presses",
    typingTime: "typing_time",
  };
  property = PROP_TRANSLATOR[property] || property;

  return PlacesUtils.withConnectionWrapper(
    "head.js::getDatabaseValue",
    async db => {
      let rows = await db.execute(
        `
        SELECT * FROM moz_places_metadata m
        JOIN moz_places h ON h.id = m.place_id
        WHERE url = :url
        ORDER BY created_at DESC
      `,
        { url }
      );
      return rows?.[0].getResultByName(property);
    }
  );
}
