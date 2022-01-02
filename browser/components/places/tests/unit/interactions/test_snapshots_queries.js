/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for queries on snapshots.
 */

add_task(async function setup() {
  // Force 2 seconds between interactions so that we can guarantee the expected
  // orders.
  let now = Date.now() - 2000 * 10;
  for (let i = 0; i < 10; i++) {
    let url = `https://example.com/${i}/`;
    await addInteractions([
      { url, created_at: now + i * 2000, updated_at: now + i * 2000 },
    ]);

    await Snapshots.add({ url: `https://example.com/${i}/` });
  }
});

add_task(async function test_query_returns_correct_order() {
  await assertSnapshots([
    { url: "https://example.com/9/" },
    { url: "https://example.com/8/" },
    { url: "https://example.com/7/" },
    { url: "https://example.com/6/" },
    { url: "https://example.com/5/" },
    { url: "https://example.com/4/" },
    { url: "https://example.com/3/" },
    { url: "https://example.com/2/" },
    { url: "https://example.com/1/" },
    { url: "https://example.com/0/" },
  ]);

  await addInteractions([{ url: "https://example.com/0/" }]);

  await assertSnapshots([
    { url: "https://example.com/0/" },
    { url: "https://example.com/9/" },
    { url: "https://example.com/8/" },
    { url: "https://example.com/7/" },
    { url: "https://example.com/6/" },
    { url: "https://example.com/5/" },
    { url: "https://example.com/4/" },
    { url: "https://example.com/3/" },
    { url: "https://example.com/2/" },
    { url: "https://example.com/1/" },
  ]);
});

add_task(async function test_query_limit() {
  await assertSnapshots(
    [{ url: `https://example.com/0/` }, { url: `https://example.com/9/` }],
    { limit: 2 }
  );
});

add_task(async function test_query_handles_tombstones() {
  let removedAt = new Date();
  await Snapshots.delete("https://example.com/3/");
  await Snapshots.delete("https://example.com/6/");

  info("Deleted snapshots should not appear in the query");
  await assertSnapshots([
    { url: "https://example.com/0/" },
    { url: "https://example.com/9/" },
    { url: "https://example.com/8/" },
    { url: "https://example.com/7/" },
    { url: "https://example.com/5/" },
    { url: "https://example.com/4/" },
    { url: "https://example.com/2/" },
    { url: "https://example.com/1/" },
  ]);

  info("Deleted snapshots should appear if tombstones are requested");
  await assertSnapshots(
    [
      { url: "https://example.com/0/" },
      { url: "https://example.com/9/" },
      { url: "https://example.com/8/" },
      { url: "https://example.com/7/" },
      { url: "https://example.com/6/", removedAt },
      { url: "https://example.com/5/" },
      { url: "https://example.com/4/" },
      { url: "https://example.com/3/", removedAt },
      { url: "https://example.com/2/" },
      { url: "https://example.com/1/" },
    ],
    { includeTombstones: true }
  );
});
