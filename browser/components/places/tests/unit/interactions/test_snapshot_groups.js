/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for snapshot groups addition, update, and removal.
 */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://example.com/12345";
const TEST_URL3 = "https://example.com/67890";
const TEST_URL4 = "https://example.com/135246";

async function delete_all_groups() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  for (let group of groups) {
    await SnapshotGroups.delete(group.id);
  }
}

/**
 * Adds interactions and snapshots for the supplied data.
 *
 * @param {string[]|InteractionInfo[]} data
 *   Either an array of urls, or an array of InteractionInfo objects suitable
 *   for passing to `addInteractions`.
 */
async function addInteractionsAndSnapshots(data) {
  for (let item of data) {
    if (typeof item == "string") {
      await addInteractions([{ url: item }]);
      await Snapshots.add({ url: item });
    } else {
      await addInteractions([item]);
      await Snapshots.add({ url: item.url });
    }
  }
}

add_task(async function setup() {
  Services.prefs.setIntPref("browser.places.snapshots.minGroupSize", 4);
});

// A group where the urls are not snapshots should exclude those urls
add_task(async function test_add_and_query_no_snapshots() {
  let urls = [TEST_URL1, TEST_URL2, TEST_URL3];
  let id = await SnapshotGroups.add(
    { title: "Group", builder: "domain" },
    urls
  );
  Assert.equal(id, 1, "id of newly added group should be 1");

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(groups[0], {
    title: "Group",
    builder: "domain",
    urls: [],
  });
});

add_task(async function test_add_and_query() {
  await delete_all_groups();

  let now = Date.now();
  let data = [
    { url: TEST_URL1, created_at: now - 30000, updated_at: now - 30000 },
    { url: TEST_URL2, created_at: now - 20000, updated_at: now - 20000 },
    { url: TEST_URL3, created_at: now - 10000, updated_at: now - 10000 },
  ];
  await addInteractionsAndSnapshots(data);

  let newGroup = { title: "Test Group", builder: "domain" };
  await SnapshotGroups.add(
    newGroup,
    data.map(d => d.url)
  );

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(groups[0], {
    title: "Test Group",
    builder: "domain",
    snapshotCount: data.length,
    lastAccessed: now - 10000,
  });
});

add_task(async function test_add_and_query_builderMetadata() {
  await delete_all_groups();
  let urls = [TEST_URL1, TEST_URL2, TEST_URL3];

  let newGroup = {
    title: "Test Group",
    builder: "domain",
    builderMetadata: { domain: "example.com" },
  };

  await SnapshotGroups.add(newGroup, urls);

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(groups[0], {
    title: "Test Group",
    builder: "domain",
    builderMetadata: { domain: "example.com" },
    snapshotCount: urls.length,
  });
});

add_task(async function test_add_and_query_with_builder() {
  await delete_all_groups();
  let urls = [TEST_URL1, TEST_URL2, TEST_URL3];

  let newGroup = {
    title: "Test Group",
    builder: "domain",
    builderMetadata: { domain: "example.com" },
  };

  await SnapshotGroups.add(newGroup, urls);

  // A query with the incorrect builder shouldn't return any groups
  let groups = await SnapshotGroups.query({
    builder: "incorrect builder",
    skipMinimum: true,
  });
  Assert.equal(groups.length, 0, "Should return 0 SnapshotGroups");

  groups = await SnapshotGroups.query({ builder: "domain", skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  assertSnapshotGroup(groups[0], {
    title: "Test Group",
    builder: "domain",
    builderMetadata: { domain: "example.com" },
    snapshotCount: urls.length,
  });
});

add_task(async function test_update_metadata() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 snapshot group");
  Assert.equal(
    groups[0].title,
    "Test Group",
    "SnapshotGroup title should be retrieved"
  );

  groups[0].title = "Modified title";
  groups[0].builder = "pinned";
  await SnapshotGroups.updateMetadata(groups[0]);

  let updated_groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(updated_groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(groups[0], {
    title: "Modified title",
    builder: "pinned",
    snapshotCount: [TEST_URL3, TEST_URL2, TEST_URL1].length,
  });
});

add_task(async function test_delete_group() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  await SnapshotGroups.delete(groups[0].id);

  groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(
    groups.length,
    0,
    "After deletion, no SnapshotGroups should be found"
  );
});

add_task(async function test_add_multiple_and_query_snapshot() {
  let urls = [TEST_URL1, TEST_URL2, TEST_URL3, TEST_URL4];
  await addInteractionsAndSnapshots(urls);

  let id = await SnapshotGroups.add(
    { title: "First Group", builder: "domain" },
    [TEST_URL1]
  );
  Assert.equal(id, 1, "id of next newly added group should be 1");

  let second_id = await SnapshotGroups.add(
    { title: "Second Group", builder: "domain" },
    [TEST_URL2]
  );
  Assert.equal(second_id, 2, "id of next newly added group should be 2");

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 2, "Should return 2 SnapshotGroups");
  assertSnapshotGroup(groups[0], {
    title: "Second Group",
    builder: "domain",
    snapshotCount: 1,
  });
  assertSnapshotGroup(groups[1], {
    title: "First Group",
    builder: "domain",
    snapshotCount: 1,
  });
});

add_task(async function test_add_and_query_no_url() {
  await delete_all_groups();
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 0, "Should return 0 SnapshotGroups");

  await SnapshotGroups.add({ title: "No url group", builder: "domain" }, []);

  let newGroups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(newGroups.length, 1, "Should return 1 SnapshotGroups");
  assertSnapshotGroup(newGroups[0], {
    title: "No url group",
    builder: "domain",
    snapshotCount: 0,
  });
});

add_task(async function test_get_snapshots() {
  await delete_all_groups();

  let urls = [TEST_URL1, TEST_URL2, TEST_URL3];
  await addInteractionsAndSnapshots(urls);

  let newGroup = { title: "Test Group", builder: "domain" };
  await SnapshotGroups.add(newGroup, urls);

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  let snapshots = await SnapshotGroups.getSnapshots({ id: groups[0].id });
  Assert.equal(snapshots.length, 3, "Should return 3 Snapshots");

  await assertSnapshotList(snapshots, [
    { url: TEST_URL3 },
    { url: TEST_URL2 },
    { url: TEST_URL1 },
  ]);
});

add_task(async function test_get_snapshots_count() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  let snapshots = await SnapshotGroups.getSnapshots({
    id: groups[0].id,
    count: 2,
  });
  Assert.equal(snapshots.length, 2, "Should return 2 Snapshots");

  await assertSnapshotList(snapshots, [{ url: TEST_URL3 }, { url: TEST_URL2 }]);
});

add_task(async function test_get_snapshots_startIndex() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  let snapshots = await SnapshotGroups.getSnapshots({
    id: groups[0].id,
    startIndex: 1,
  });
  Assert.equal(snapshots.length, 2, "Should return 2 Snapshots");

  await assertSnapshotList(snapshots, [{ url: TEST_URL2 }, { url: TEST_URL1 }]);
});

add_task(async function test_minimum_size() {
  let groups = await SnapshotGroups.query();
  Assert.equal(
    groups.length,
    0,
    "Should return no groups when they are under the snapshot size limit."
  );

  // TODO: Ideally this would use `updateUrls` to update 'Test Group' but that
  // api is not implemented yet.
  let newGroup = { title: "Test Group 2", builder: "domain" };
  await SnapshotGroups.add(newGroup, [
    TEST_URL1,
    TEST_URL2,
    TEST_URL3,
    TEST_URL4,
  ]);

  groups = await SnapshotGroups.query();
  Assert.equal(
    groups.length,
    1,
    "Should have returned one group above the snapshot size limit"
  );
  assertSnapshotGroup(groups[0], {
    title: "Test Group 2",
    builder: "domain",
    snapshotCount: 4,
  });
});
