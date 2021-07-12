/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for queries on snapshots.
 */

add_task(async function setup() {
  for (let i = 0; i < 10; i++) {
    let url = `https://example.com/${i}/`;
    await addInteractions([{ url }]);

    await Snapshots.add({ url: `https://example.com/${i}/` });

    // Give a little time between updates to help the ordering.
    /* eslint-disable-next-line mozilla/no-arbitrary-setTimeout */
    await new Promise(r => setTimeout(r, 20));
  }
});

add_task(async function test_query_returns_correct_order() {
  await assertSnapshots([
    { url: "https://example.com/0/" },
    { url: "https://example.com/1/" },
    { url: "https://example.com/2/" },
    { url: "https://example.com/3/" },
    { url: "https://example.com/4/" },
    { url: "https://example.com/5/" },
    { url: "https://example.com/6/" },
    { url: "https://example.com/7/" },
    { url: "https://example.com/8/" },
    { url: "https://example.com/9/" },
  ]);

  // TODO: Update an interaction, and check the new order is returned corectly.
});

add_task(async function test_query_limit() {
  await assertSnapshots(
    [{ url: `https://example.com/0/` }, { url: `https://example.com/1/` }],
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
    { url: "https://example.com/1/" },
    { url: "https://example.com/2/" },
    { url: "https://example.com/4/" },
    { url: "https://example.com/5/" },
    { url: "https://example.com/7/" },
    { url: "https://example.com/8/" },
    { url: "https://example.com/9/" },
  ]);

  info("Deleted snapshots should appear if tombstones are requested");
  await assertSnapshots(
    [
      { url: "https://example.com/0/" },
      { url: "https://example.com/1/" },
      { url: "https://example.com/2/" },
      { url: "https://example.com/3/", removedAt },
      { url: "https://example.com/4/" },
      { url: "https://example.com/5/" },
      { url: "https://example.com/6/", removedAt },
      { url: "https://example.com/7/" },
      { url: "https://example.com/8/" },
      { url: "https://example.com/9/" },
    ],
    { includeTombstones: true }
  );
});
