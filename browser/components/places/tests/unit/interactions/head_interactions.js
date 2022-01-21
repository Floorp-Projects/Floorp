/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Interactions: "resource:///modules/Interactions.jsm",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  Services: "resource://gre/modules/Services.jsm",
  SnapshotGroups: "resource:///modules/SnapshotGroups.jsm",
  Snapshots: "resource:///modules/Snapshots.jsm",
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
  await PlacesTestUtils.addVisits(interactions.map(i => i.url));

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

      if (expected.every(url => arr.includes(url))) {
        seen = true;
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
 * 24 hours prior to the current date.
 *
 * @param {Date} date
 *   The date to check.
 */
function assertRecentDate(date) {
  Assert.greater(
    date.getTime(),
    Date.now() - 1000 * 60 * 60 * 24,
    "Should have a reasonable value for the date"
  );
}

/**
 * Asserts that an individual snapshot contains the expected values.
 *
 * @param {Snapshot} actual
 *  The snapshot to test.
 * @param {Snapshot} expected
 *  The snapshot to test against.
 */
function assertSnapshot(actual, expected) {
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
  assertRecentDate(actual.createdAt);
  assertRecentDate(actual.lastInteractionAt);
  if (actual.firstInteractionAt || !actual.userPersisted) {
    // If a snapshot is manually created before its corresponding interaction is
    // created, we assign a temporary value of 0 for first_interaction_at. In
    // all other cases, we want to ensure a reasonable date value is being used.
    assertRecentDate(actual.firstInteractionAt);
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
  if (expected.overlappingVisitScoreIs != null) {
    Assert.equal(
      actual.overlappingVisitScore,
      expected.overlappingVisitScoreIs,
      "Should have an overlappingVisitScore equal to the expected score"
    );
  }
  if (expected.overlappingVisitScoreGreaterThan != null) {
    Assert.greater(
      actual.overlappingVisitScore,
      expected.overlappingVisitScoreGreaterThan,
      "Should have an overlappingVisitScore greater than the expected score"
    );
  }
  if (expected.overlappingVisitScoreLessThan != null) {
    Assert.less(
      actual.overlappingVisitScore,
      expected.overlappingVisitScoreLessThan,
      "Should have an overlappingVisitScore less than the expected score"
    );
  }
  if (expected.overlappingVisitScoreLessThanEqualTo != null) {
    Assert.lessOrEqual(
      actual.overlappingVisitScore,
      expected.overlappingVisitScoreLessThanEqualTo,
      "Should have an overlappingVisitScore less than or equal to the expected score"
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
  if (expected.commonReferrerScoreEqualTo != null) {
    Assert.equal(
      actual.commonReferrerScore,
      expected.commonReferrerScoreEqualTo,
      "Should have a commonReferrerScore equal to the expected score"
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
  if (expected.title != null) {
    Assert.equal(group.title, expected.title, "Should have the expected title");
  }
  if (expected.builder != null) {
    Assert.equal(
      group.builder,
      expected.builder,
      "Should have the expected builder"
    );
  }
  if (expected.builderMetadata != null) {
    Assert.deepEqual(
      group.builderMetadata,
      expected.builderMetadata,
      "Should have the expected builderMetadata"
    );
  }
  if (expected.snapshotCount != null) {
    Assert.equal(
      group.snapshotCount,
      expected.snapshotCount,
      "Should have the expected snapshotCount"
    );
  }
}

/**
 * Queries overlapping snapshots from the database and asserts their expected values.
 *
 * @param {Snapshot[]} expected
 *   The expected snapshots.
 * @param {object} context
 *   @see SnapshotSelector.#context.
 */
async function assertOverlappingSnapshots(expected, context) {
  let snapshots = await Snapshots.queryOverlapping(context.url);

  await assertSnapshotList(snapshots, expected);
}

/**
 * Queries common referrer snapshots from the database and asserts their expected values.
 *
 * @param {Snapshot[]} expected
 *   The expected snapshots.
 * @param {object} context
 *   @see SnapshotSelector.#context.
 */
async function assertCommonReferrerSnapshots(expected, context) {
  let snapshots = await Snapshots.queryCommonReferrer(
    context.url,
    context.referrerUrl
  );

  await assertSnapshotList(snapshots, expected);
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
 * @param {Snapshot[]} combinedSnapshots
 *   The array of combined snapshots.
 * @param {object[]} expectedSnapshots
 *   An array of objects containing expected url and relevancyScore properties.
 */
function assertSnapshotScores(combinedSnapshots, expectedSnapshots) {
  Assert.equal(
    combinedSnapshots.length,
    expectedSnapshots.length,
    "Should have returned the correct amount of snapshots"
  );

  for (let i = 0; i < combinedSnapshots.length; i++) {
    Assert.equal(
      combinedSnapshots[i].url,
      expectedSnapshots[i].url,
      "Should have returned the expected URL for the snapshot"
    );
    Assert.equal(
      combinedSnapshots[i].relevancyScore,
      expectedSnapshots[i].score,
      `Should have set the expected score for ${expectedSnapshots[i].url}`
    );
  }
}
