/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Interactions: "resource:///modules/Interactions.jsm",
  PageThumbs: "resource://gre/modules/PageThumbs.jsm",
  PlacesPreviews: "resource://gre/modules/PlacesPreviews.jsm",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  SnapshotGroups: "resource:///modules/SnapshotGroups.jsm",
  Snapshots: "resource:///modules/Snapshots.jsm",
  SnapshotMonitor: "resource:///modules/SnapshotMonitor.jsm",
  SnapshotScorer: "resource:///modules/SnapshotScorer.jsm",
  SnapshotSelector: "resource:///modules/SnapshotSelector.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
});

// Initialize profile.
var gProfD = do_get_profile(true);
Services.prefs.setBoolPref("browser.pagethumbnails.capturing_disabled", true);

// Observer notifications.
const TOPIC_ADDED = "places-snapshots-added";
const TOPIC_DELETED = "places-snapshots-deleted";

/**
 * Adds a test interaction to the database.
 *
 * @param {InteractionInfo[]} interactions
 */
async function addInteractions(interactions) {
  await PlacesTestUtils.addVisits(interactions);

  for (let interaction of interactions) {
    await Interactions.store.add({
      url: interaction.url,
      title: interaction.title,
      documentType:
        interaction.documentType ?? Interactions.DOCUMENT_TYPE.GENERIC,
      totalViewTime: interaction.totalViewTime ?? 0,
      typingTime: interaction.typingTime ?? 0,
      keypresses: interaction.keypresses ?? 0,
      scrollingTime: interaction.scrollingTime ?? 0,
      scrollingDistance: interaction.scrollingDistance ?? 0,
      created_at: interaction.created_at || Date.now(),
      updated_at: interaction.updated_at || Date.now(),
      referrer: interaction.referrer || "",
    });
  }
  await Interactions.store.flush();
}

/**
 * Fetches the current metadata from the database.
 */
async function getInteractions() {
  const columns = [
    "id",
    "place_id",
    "url",
    "updated_at",
    "total_view_time",
    "typing_time",
    "key_presses",
  ];
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.executeCached(
    `SELECT m.id AS id, h.id AS place_id, h.url AS url, updated_at,
            total_view_time, typing_time, key_presses
     FROM moz_places_metadata m
     JOIN moz_places h ON h.id = m.place_id
     ORDER BY updated_at DESC`
  );
  return rows.map(r => {
    let result = {};
    for (let column of columns) {
      result[column] = r.getResultByName(column);
    }
    return result;
  });
}

/**
 * Executes an async task and verifies that the given notification was sent with
 * the given list of urls.
 *
 * @param {string} topic
 * @param {string[]} expected
 * @param {function} task
 */
async function assertUrlNotification(topic, expected, task) {
  let seen = false;

  let listener = (subject, _, data) => {
    try {
      let arr = JSON.parse(data);
      if (arr.length != expected.length) {
        return;
      }
      if (topic == TOPIC_ADDED) {
        // For the added notification we receive objects including the url
        // and userPersisted.
        seen = arr.every(item => expected.includes(item.url));
      } else {
        seen = arr.every(url => expected.includes(url));
      }
    } catch (e) {
      Assert.ok(false, e);
    }
  };

  Services.obs.addObserver(listener, topic);
  await task();
  Services.obs.removeObserver(listener, topic);

  Assert.ok(seen, `Should have seen ${topic} notification.`);
}

/**
 * Executes an async task and verifies that the given observer notification was
 * not sent.
 *
 * @param {string} topic
 * @param {function} task
 */
async function assertTopicNotObserved(topic, task) {
  let seen = false;

  let listener = () => {
    seen = true;
  };

  Services.obs.addObserver(listener, topic);
  await task();
  Services.obs.removeObserver(listener, topic);

  Assert.ok(!seen, `Should not have seen ${topic} notification.`);
}

/**
 * Asserts that a date looks reasonably valid, i.e. created no earlier than
 * threshold days prior to the current date.
 *
 * @param {Date} date
 *   The date to check.
 * @param {number} threshold
 *   Maximum number of days the date should be in the past.
 */
function assertRecentDate(date, threshold = 1) {
  Assert.greater(
    date.getTime(),
    Date.now() - 1000 * 60 * 60 * 24 * threshold,
    "Should have a reasonable value for the date"
  );
}

/**
 * Asserts that an individual snapshot contains the expected values.
 *
 * @param {Snapshot|Recommendation} actual
 *  The snapshot to test.
 * @param {Snapshot} expected
 *  The snapshot to test against.
 */
function assertSnapshot(actual, expected) {
  // This may be a recommendation.
  let score = 0;
  let source = null;
  if ("snapshot" in actual) {
    score = actual.score;
    source = actual.source;
    actual = actual.snapshot;
  }

  Assert.equal(actual.url, expected.url, "Should have the expected URL");
  let expectedTitle = `test visit for ${expected.url}`;
  if (expected.hasOwnProperty("title")) {
    // We set title in this statement instead of with an OR so that consumers
    // can pass an explicit null.
    expectedTitle = expected.title;
  }
  Assert.equal(actual.title, expectedTitle, "Should have the expected title");
  // Avoid falsey-types that we might get from the database.
  Assert.strictEqual(
    actual.userPersisted,
    expected.userPersisted ?? Snapshots.USER_PERSISTED.NO,
    "Should have the expected user persisted value"
  );
  Assert.strictEqual(
    actual.documentType,
    expected.documentType ?? Interactions.DOCUMENT_TYPE.GENERIC,
    "Should have the expected document type"
  );
  assertRecentDate(actual.createdAt, expected.daysThreshold);
  assertRecentDate(actual.lastInteractionAt, expected.daysThreshold);
  if (actual.firstInteractionAt || !actual.userPersisted) {
    // If a snapshot is manually created before its corresponding interaction is
    // created, we assign a temporary value of 0 for first_interaction_at. In
    // all other cases, we want to ensure a reasonable date value is being used.
    assertRecentDate(actual.firstInteractionAt, expected.daysThreshold);
  }
  if (expected.lastUpdated) {
    Assert.greaterOrEqual(
      actual.lastInteractionAt,
      expected.lastUpdated,
      "Should have a last interaction time greater than or equal to the expected last updated time"
    );
  }
  if (expected.commonName) {
    Assert.equal(
      actual.commonName,
      expected.commonName,
      "Should have the Snapshot URL's common name."
    );
  }
  if (expected.source) {
    Assert.equal(
      source,
      expected.source,
      "Should have the correct recommendation source."
    );
  }
  if (expected.scoreEqualTo != null) {
    Assert.equal(
      score,
      expected.scoreEqualTo,
      "Should have a score equal to the expected score"
    );
  }
  if (expected.scoreGreaterThan != null) {
    Assert.greater(
      score,
      expected.scoreGreaterThan,
      "Should have a score greater than the expected score"
    );
  }
  if (expected.scoreLessThan != null) {
    Assert.less(
      score,
      expected.scoreLessThan,
      "Should have a score less than the expected score"
    );
  }
  if (expected.scoreLessThanEqualTo != null) {
    Assert.lessOrEqual(
      score,
      expected.scoreLessThanEqualTo,
      "Should have a score less than or equal to the expected score"
    );
  }
  if (expected.removedAt) {
    Assert.greaterOrEqual(
      actual.removedAt.getTime(),
      expected.removedAt.getTime(),
      "Should have the removed at time greater than or equal to the expected removed at time"
    );
  } else {
    Assert.strictEqual(
      actual.removedAt,
      null,
      "Should not have a removed at time"
    );
  }
}

/**
 * Asserts that the list of snapshots match the expected values.
 *
 * @param {Snapshot[]} received
 *   The received snapshots.
 * @param {Snapshot[]} expected
 *   The expected snapshots.
 */
async function assertSnapshotList(received, expected) {
  info(`Found ${received.length} snapshots:\n ${JSON.stringify(received)}`);
  Assert.equal(
    received.length,
    expected.length,
    "Should have the expected number of snapshots"
  );
  for (let i = 0; i < expected.length; i++) {
    assertSnapshot(received[i], expected[i]);
  }
}

/**
 * Asserts that the snapshots in the database match the expected values.
 *
 * @param {Snapshot[]} expected
 *   The expected snapshots.
 * @param {object} options
 *   @see Snapshots.query().
 */
async function assertSnapshots(expected, options) {
  let snapshots = await Snapshots.query(options);

  await assertSnapshotList(snapshots, expected);
}

/**
 * Asserts that the snapshot groups match the expected values.
 *
 * @param {SnapshotGroup} group
 *   The actual snapshot groups.
 * @param {SnapshotGroup} expected
 *   The expected snapshot group.
 */
function assertSnapshotGroup(group, expected) {
  for (let [p, v] of Object.entries(expected)) {
    let comparator = Assert.equal.bind(Assert);
    if (v && typeof v == "object") {
      comparator = Assert.deepEqual.bind(Assert);
    }
    comparator(group[p], v, `Should have the expected ${p} value`);
  }
}

/**
 * Asserts that the list of snapshot groups match the expected values.
 *
 * @param {SnapshotGroup[]} received
 *   The received snapshots.
 * @param {SnapshotGroup[]} expected
 *   The expected snapshots.
 */
async function assertSnapshotGroupList(received, expected) {
  info(
    `Found ${received.length} snapshot groups:\n ${JSON.stringify(received)}`
  );
  Assert.equal(
    received.length,
    expected.length,
    "Should have the expected number of snapshots"
  );
  for (let i = 0; i < expected.length; i++) {
    assertSnapshotGroup(received[i], expected[i]);
  }
}

/**
 * Given a list of snapshot groups and a list of ids returns the groups in that
 * order.
 *
 * @param {SnapshotGroup[]} list
 *   The list of groups.
 * @param {string[]} order
 *   The ids of groups.
 * @returns {SnapshotGroup[]} The list of groups in the order expected.
 */
function orderedGroups(list, order) {
  let groups = Object.fromEntries(list.map(g => [g.id, g]));
  return order.map(id => groups[id]);
}

/**
 * Queries overlapping snapshots from the database and asserts their expected values.
 *
 * @param {Snapshot[]} expected
 *   The expected snapshots.
 * @param {SelectionContext} context
 *   @see SnapshotSelector.#context.
 */
async function assertOverlappingSnapshots(expected, context) {
  let recommendations = await Snapshots.recommendationSources.Overlapping(
    context
  );

  await assertSnapshotList(recommendations, expected);
}

/**
 * Queries common referrer snapshots from the database and asserts their expected values.
 *
 * @param {Snapshot[]} expected
 *   The expected snapshots.
 * @param {SelectionContext} context
 *   @see SnapshotSelector.#context.
 */
async function assertCommonReferrerSnapshots(expected, context) {
  let recommendations = await Snapshots.recommendationSources.CommonReferrer(
    context
  );

  await assertSnapshotList(recommendations, expected);
}

/**
 * Queries time of day snapshots from the database and asserts their expected values.
 *
 * @param {Snapshot[]} expected
 *   The expected snapshots.
 * @param {SelectionContext} context
 *   @see SnapshotSelector.#context.
 */
async function assertTimeOfDaySnapshots(expected, context) {
  let recommendations = await Snapshots.recommendationSources.TimeOfDay(
    context
  );

  await assertSnapshotList(recommendations, expected);
}

/**
 * Clears all data from the snapshots and metadata tables.
 */
async function reset() {
  await Snapshots.reset();
  await Interactions.reset();
}

/**
 * Asserts relevancy scores for snapshots are correct.
 *
 * @param {Recommendation[]} recommendations
 *   The array of combined snapshots.
 * @param {object[]} expected
 *   An array of objects containing expected url and score properties.
 */
function assertRecommendations(recommendations, expected) {
  Assert.equal(
    recommendations.length,
    expected.length,
    "Should have returned the correct amount of recommendations"
  );

  for (let i = 0; i < recommendations.length; i++) {
    Assert.equal(
      recommendations[i].snapshot.url,
      expected[i].url,
      "Should have returned the expected URL for the recommendation"
    );
    Assert.equal(
      recommendations[i].score,
      expected[i].score,
      `Should have set the expected score for ${expected[i].url}`
    );

    if (expected[i].source) {
      Assert.equal(
        recommendations[i].source,
        expected[i].source,
        `Should have set the correct source for ${expected[i].url}`
      );
    }
  }
}

/**
 * Abstracts getting the right moz-page-thumb url depending on enabled features.
 * @param {string} url
 *  The page url to get a screenshot for.
 * @returns {string} a moz-page-thumb:// url
 */
function getPageThumbURL(url) {
  if (PlacesPreviews.enabled) {
    return PlacesPreviews.getPageThumbURL(url);
  }
  return PageThumbs.getThumbnailURL(url);
}
