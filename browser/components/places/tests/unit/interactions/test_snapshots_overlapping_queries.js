/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for queries on overlapping snapshots (i.e. those whose interactions overlapped within a the given overlap limit)
 */

const ONE_MINUTE = 1000 * 60;

const overlapLimit = Services.prefs.getIntPref(
  "browser.places.interactions.snapshotOverlapLimit",
  1800000
); // 1000 * 60 * 30

/**
 * Creates snapshots with a specified delay between their interactions.
 * Urls will be in the form https://example.com/${i}/ where https://example.com/${i}/
 * will be the first interaction.
 *
 * @param {object} [options]
 * @param {number} [options.num]
 *   Number of snapshots to create
 * @param {number} [options.duration]
 *   The duration, in milliseconds, of each snapshot's interaction
 * @param {number} [options.delay]
 *   The delay, in milliseconds, between the interactions
 * @param {number} [options.currentTime]
 *   The base time to create the interactions at. Delays are added per iteration.
 */
async function create_snapshots({
  num = 1,
  duration = 5000,
  delay = 2000,
  currentTime = Date.now(),
} = {}) {
  let now = currentTime - delay * num;
  for (let i = 0; i < num; i++) {
    let url = `https://example.com/${i}/`;
    await addInteractions([
      {
        url,
        created_at: now + i * delay,
        updated_at: now + i * delay + duration,
      },
    ]);

    await Snapshots.add({ url: `https://example.com/${i}/` });
  }
}

/**
 * Reset all interactions and snapshots
 *
 */
async function reset_interactions_snapshots() {
  await Interactions.reset();
  await Snapshots.reset();
}

// Test snapshots where the interactions were spaced beyond the overlap limit
add_task(async function test_query_multiple_context_no_overlap() {
  await reset_interactions_snapshots();

  await create_snapshots({ num: 10, delay: overlapLimit * 2.0 });

  for (let i = 0; i < 10; i++) {
    // Test each url as the context
    let context = { url: `https://example.com/${i}/` };
    await assertSnapshotsWithContext([], context);
  }
});

// Test a single snapshot against overlapping snapshots
add_task(async function test_query_single_context_overlap() {
  await reset_interactions_snapshots();

  // Long interactions so that they all overlap
  await create_snapshots({ num: 10, duration: overlapLimit });

  // Use the first url as the context
  let context = { url: "https://example.com/0/" };

  await assertSnapshotsWithContext(
    [
      { url: "https://example.com/9/", overlappingVisitScoreGreaterThan: 0 },
      { url: "https://example.com/8/", overlappingVisitScoreGreaterThan: 0 },
      { url: "https://example.com/7/", overlappingVisitScoreGreaterThan: 0 },
      { url: "https://example.com/6/", overlappingVisitScoreGreaterThan: 0 },
      { url: "https://example.com/5/", overlappingVisitScoreGreaterThan: 0 },
      { url: "https://example.com/4/", overlappingVisitScoreGreaterThan: 0 },
      { url: "https://example.com/3/", overlappingVisitScoreGreaterThan: 0 },
      { url: "https://example.com/2/", overlappingVisitScoreGreaterThan: 0 },
      { url: "https://example.com/1/", overlappingVisitScoreGreaterThan: 0 },
    ],
    context
  );
});

// Test finding snapshots where the context url is not a snapshot and had no interactions (none should be found)
add_task(async function test_query_context_not_a_snapshot_no_interaction() {
  await reset_interactions_snapshots();

  let context_url = "https://example.com/0/";

  let now = Date.now();
  let test_url = "https://example.com/1/";
  await addInteractions([{ url: test_url, created_at: now, updated_at: now }]);
  await Snapshots.add({ url: test_url });

  await assertSnapshotsWithContext([], { url: context_url });
});

// Test finding snapshots where the context url is not a snapshot but had an overlapping interaction (the test snapshot should be found)
add_task(async function test_query_context_not_a_snapshot() {
  await reset_interactions_snapshots();

  let now = Date.now();
  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);

  let test_url = "https://example.com/1/";
  await addInteractions([{ url: test_url, created_at: now, updated_at: now }]);
  await Snapshots.add({ url: test_url });

  await assertSnapshotsWithContext(
    [{ url: "https://example.com/1/", overlappingVisitScoreGreaterThan: 0 }],
    { url: context_url }
  );
});

// Test finding snapshots where the context url is not a snapshot and the test url had interactions but was not a snapshot (none should be found)
add_task(
  async function test_query_context_not_a_snapshot_test_not_a_snapshot() {
    await reset_interactions_snapshots();

    let now = Date.now();
    let context_url = "https://example.com/0/";
    await addInteractions([
      { url: context_url, created_at: now, updated_at: now },
    ]);

    let test_url = "https://example.com/1/";
    await addInteractions([
      { url: test_url, created_at: now, updated_at: now },
    ]);

    await assertSnapshotsWithContext([], { url: context_url });
  }
);

// Test finding snapshots where the context url is a snapshot and the test url had interactions but was not a snapshot (none should be found)
add_task(async function test_query_context_a_snapshot_test_not_a_snapshot() {
  await reset_interactions_snapshots();

  let now = Date.now();
  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);
  await Snapshots.add({ url: context_url });

  let test_url = "https://example.com/1/";
  await addInteractions([{ url: test_url, created_at: now, updated_at: now }]);

  await assertSnapshotsWithContext([], { url: context_url });
});

// Test adding an interaction one minute before the overlap limit (should not be found)
add_task(async function test_query_interact_before_no_overlap() {
  await reset_interactions_snapshots();

  let now = Date.now();
  let before = now - overlapLimit - ONE_MINUTE;
  let test_url = "https://example.com/1/";
  await addInteractions([
    { url: test_url, created_at: before, updated_at: before },
  ]);
  await Snapshots.add({ url: test_url });

  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);
  await Snapshots.add({ url: context_url });

  await assertSnapshotsWithContext([], { url: context_url });
});

// Test adding an interaction before the context url, within one minute of the overlap limit (should be found, with a low score)
add_task(async function test_query_interact_before_overlap() {
  await reset_interactions_snapshots();

  let now = Date.now();
  let before = now - overlapLimit + ONE_MINUTE;
  let test_url = "https://example.com/1/";
  await addInteractions([
    { url: test_url, created_at: before, updated_at: before },
  ]);
  await Snapshots.add({ url: test_url });

  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);
  await Snapshots.add({ url: context_url });

  await assertSnapshotsWithContext(
    [
      {
        url: "https://example.com/1/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: 0.1,
      },
    ],
    { url: context_url }
  );
});

// Test adding an interaction one minute past the overlap limit (should not be found)
add_task(async function test_query_interact_after_no_overlap() {
  await reset_interactions_snapshots();

  let now = Date.now();

  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);
  await Snapshots.add({ url: context_url });

  let after = now + overlapLimit + ONE_MINUTE;
  let test_url = "https://example.com/1/";
  await addInteractions([
    { url: test_url, created_at: after, updated_at: after },
  ]);
  await Snapshots.add({ url: test_url });

  await assertSnapshotsWithContext([], { url: context_url });
});

// Test adding an interaction one minute less than the overlap limit (should be found, with a low score)
add_task(async function test_query_interact_after_overlap() {
  await reset_interactions_snapshots();

  let now = Date.now();

  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);
  await Snapshots.add({ url: context_url });

  let after = now + overlapLimit - ONE_MINUTE;
  let test_url = "https://example.com/1/";
  await addInteractions([
    { url: test_url, created_at: after, updated_at: after },
  ]);
  await Snapshots.add({ url: test_url });

  // The interaction should be near the lower end of the [0, 1] scoring range
  await assertSnapshotsWithContext(
    [
      {
        url: "https://example.com/1/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: 0.1,
      },
    ],
    { url: context_url }
  );
});

// Test adding an interaction one minute after the context url's interaction (should be found, with a higher score)
add_task(async function test_query_interact_after_overlap() {
  await reset_interactions_snapshots();

  let now = Date.now();

  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);
  await Snapshots.add({ url: context_url });

  let after = now + ONE_MINUTE;
  let test_url = "https://example.com/1/";
  await addInteractions([
    { url: test_url, created_at: after, updated_at: after },
  ]);
  await Snapshots.add({ url: test_url });

  // The interaction should be near the upper end of the [0, 1] scoring range
  await assertSnapshotsWithContext(
    [
      {
        url: "https://example.com/1/",
        overlappingVisitScoreGreaterThan: 0.9,
        overlappingVisitScoreLessThan: 1.0,
      },
    ],
    { url: context_url }
  );
});

// Test a single snapshot against snapshots that are progressively further away
add_task(async function test_query_single_context_increasing() {
  await reset_interactions_snapshots();

  await create_snapshots({ num: 10, duration: 1000, delay: 1000 * 60 });

  // Use the first url as the context
  let context = { url: "https://example.com/0/" };

  let snapshots = await Snapshots.queryOverlapping(context.url);

  // Ensure interactions closer to the context have higher scores
  let scores = {};
  for (let s of snapshots) {
    scores[s.url] = s.overlappingVisitScore;
  }
  await assertSnapshotsWithContext(
    [
      { url: "https://example.com/1/", overlappingVisitScoreGreaterThan: 0.0 },
      {
        url: "https://example.com/2/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: scores["https://example.com/1/"],
      },
      {
        url: "https://example.com/3/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: scores["https://example.com/2/"],
      },
      {
        url: "https://example.com/4/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: scores["https://example.com/3/"],
      },
      {
        url: "https://example.com/5/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: scores["https://example.com/4/"],
      },
      {
        url: "https://example.com/6/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: scores["https://example.com/5/"],
      },
      {
        url: "https://example.com/7/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: scores["https://example.com/6/"],
      },
      {
        url: "https://example.com/8/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: scores["https://example.com/7/"],
      },
      {
        url: "https://example.com/9/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: scores["https://example.com/8/"],
      },
    ],
    context
  );
});

// Test that revisiting an overlapping site increases its score, but not greater than 1.0
add_task(async function test_query_revisit() {
  await reset_interactions_snapshots();

  let now = Date.now();
  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);
  await Snapshots.add({ url: context_url });

  // Add the test interaction near the end of the range
  let after = now + overlapLimit - ONE_MINUTE;
  await addInteractions([
    { url: "https://example.com/1/", created_at: after, updated_at: after },
  ]);
  await Snapshots.add({ url: "https://example.com/1/" });

  // The interaction should be near the lower end of the [0, 1] scoring range
  await assertSnapshotsWithContext(
    [
      {
        url: "https://example.com/1/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThan: 1.0,
      },
    ],
    { url: context_url }
  );

  let snapshots = await Snapshots.queryOverlapping(context_url);
  let scores = {};
  for (let s of snapshots) {
    scores[s.url] = s.overlappingVisitScore;
  }

  // Add another interaction: the score should increase (but not exceed 1.0)
  await addInteractions([
    {
      url: "https://example.com/1/",
      created_at: after - ONE_MINUTE,
      updated_at: after - ONE_MINUTE,
    },
  ]);
  await Snapshots.add({ url: "https://example.com/1/" });

  await assertSnapshotsWithContext(
    [
      {
        url: "https://example.com/1/",
        overlappingVisitScoreGreaterThan: scores["https://example.com/1/"],
        overlappingVisitScoreLessThanEqualTo: 1.0,
      },
    ],
    { url: context_url }
  );

  // Add another interaction: the score should increase (but still not exceed 1.0)
  snapshots = await Snapshots.queryOverlapping(context_url);
  scores = {};
  for (let s of snapshots) {
    scores[s.url] = s.overlappingVisitScore;
  }

  await addInteractions([
    {
      url: "https://example.com/1/",
      created_at: now + overlapLimit / 2.0,
      updated_at: now + overlapLimit / 2.0,
    },
  ]);
  await Snapshots.add({ url: "https://example.com/1/" });

  await assertSnapshotsWithContext(
    [
      {
        url: "https://example.com/1/",
        overlappingVisitScoreGreaterThan: scores["https://example.com/1/"],
        overlappingVisitScoreLessThanEqualTo: 1.0,
      },
    ],
    { url: context_url }
  );
});

// Test that revisiting an overlapping site numerous times does not lead to a score greater than 1.0
add_task(async function test_query_numerous_revisit() {
  await reset_interactions_snapshots();

  let now = Date.now();
  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);
  await Snapshots.add({ url: context_url });

  for (let i = 0; i < 10; i++) {
    let after = now + (overlapLimit / 10.0) * i;
    await addInteractions([
      { url: "https://example.com/1/", created_at: after, updated_at: after },
    ]);
    await Snapshots.add({ url: "https://example.com/1/" });
  }

  // The interaction should be near the lower end of the [0, 1] scoring range
  await assertSnapshotsWithContext(
    [
      {
        url: "https://example.com/1/",
        overlappingVisitScoreGreaterThan: 0.0,
        overlappingVisitScoreLessThanEqualTo: 1.0,
      },
    ],
    { url: context_url }
  );
});
