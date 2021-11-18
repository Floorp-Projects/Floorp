/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for queries on overlapping snapshots (i.e. those whose interactions overlapped within a half-hour on each end)
 */

const ONE_MINUTE = 1000 * 60;
const ONE_HOUR = ONE_MINUTE * 60;

/**
 * Creates snapshots with a specified delay between their interactions.
 * Urls will be in the form https://example.com/${i}/ where https://example.com/${i}/
 * will be the first interaction.
 *
 * @param {number} num
 *   Number of snapshots to create
 * @param {number} delay
 *   The delay, in milliseconds, between each snapshot's interaction
 * @param {number} currentTime
 *   The base time to create the interactions at. Delays are added per iteration.
 */
async function create_snapshots(num, delay, currentTime = Date.now()) {
  let now = currentTime - delay * num;
  for (let i = 0; i < num; i++) {
    let url = `https://example.com/${i}/`;
    await addInteractions([
      { url, created_at: now + i * delay, updated_at: now + i * delay },
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

// Test the snapshots against each other
add_task(async function test_query_multiple_context_no_overlap() {
  await reset_interactions_snapshots();

  await create_snapshots(10, ONE_HOUR);

  for (let i = 0; i < 10; i++) {
    // Test each url as the context
    let context = { url: `https://example.com/${i}/` };
    await assertSnapshotsWithContext([], context);
  }
});

// Test a single snapshot against the others
add_task(async function test_query_single_context_overlap() {
  await reset_interactions_snapshots();

  await create_snapshots(10, 2000);

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

// Test adding an interaction one minute past a half hour before (should not be found)
add_task(async function test_query_interact_before_no_overlap() {
  await reset_interactions_snapshots();

  let now = Date.now();
  let before = now - ONE_HOUR / 2 - ONE_MINUTE;
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

// Test adding an interaction one minute less than half an hour before (should be found)
add_task(async function test_query_interact_before_overlap() {
  await reset_interactions_snapshots();

  let now = Date.now();
  let before = now - ONE_HOUR / 2 + ONE_MINUTE;
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
    [{ url: "https://example.com/1/", overlappingVisitScoreGreaterThan: 0 }],
    { url: context_url }
  );
});

// Test adding an interaction one minute past a half hour after (should not be found)
add_task(async function test_query_interact_after_no_overlap() {
  await reset_interactions_snapshots();

  let now = Date.now();
  let after = now + ONE_HOUR / 2 + ONE_MINUTE;
  let test_url = "https://example.com/1/";
  await addInteractions([
    { url: test_url, created_at: after, updated_at: after },
  ]);
  await Snapshots.add({ url: test_url });

  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);
  await Snapshots.add({ url: context_url });

  await assertSnapshotsWithContext([], { url: context_url });
});

// Test adding an interaction one minute less than half an hour after (should be found)
add_task(async function test_query_interact_after_overlap() {
  await reset_interactions_snapshots();

  let now = Date.now();
  let after = now + ONE_HOUR / 2 - ONE_MINUTE;
  let test_url = "https://example.com/1/";
  await addInteractions([
    { url: test_url, created_at: after, updated_at: after },
  ]);
  await Snapshots.add({ url: test_url });

  let context_url = "https://example.com/0/";
  await addInteractions([
    { url: context_url, created_at: now, updated_at: now },
  ]);
  await Snapshots.add({ url: context_url });

  await assertSnapshotsWithContext(
    [{ url: "https://example.com/1/", overlappingVisitScoreGreaterThan: 0 }],
    { url: context_url }
  );
});
