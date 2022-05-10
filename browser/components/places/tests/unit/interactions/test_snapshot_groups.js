/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for snapshot groups addition, update, and removal.
 */

const FAVICON_DATAURL1 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";
const FAVICON_DATAURL2 =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVBBBBCklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";
const TEST_URL1 = "https://example.com/";
const TEST_IMAGE_URL1 = "https://example.com/preview1.png";
const TEST_URL2 = "https://example.com/12345";
const TEST_IMAGE_URL2 = "https://example.com/preview2.png";
const TEST_URL3 = "https://example.com/67890";
const TEST_URL4 = "https://example.com/135246";
const TEST_URL5 = "https://example.com/531246";
const TEST_PINNED_URL = "https://example.com/pinned";

async function delete_all_groups() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  for (let group of groups) {
    await SnapshotGroups.delete(group.id);
  }
}

async function setIcon(pageUrl, iconUrl, dataUrl) {
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    Services.io.newURI(iconUrl),
    dataUrl,
    (Date.now() + 8640000) * 1000,
    Services.scriptSecurityManager.getSystemPrincipal()
  );

  await new Promise(resolve => {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      Services.io.newURI(pageUrl),
      Services.io.newURI(iconUrl),
      false,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      resolve,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  });
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
      await Snapshots.add({
        url: item.url,
        userPersisted: item.userPersisted ?? Snapshots.USER_PERSISTED.NO,
      });
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
    builderMetadata: { title: "Group" },
    snapshotCount: 0,
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

  info("add a favicon for both urls");
  await setIcon(TEST_URL1, TEST_URL1 + "favicon.ico", FAVICON_DATAURL1);
  await setIcon(TEST_URL2, TEST_URL2 + "/favicon.ico", FAVICON_DATAURL2);

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(groups[0], {
    title: "Test Group",
    builder: "domain",
    builderMetadata: { title: "Test Group" },
    hidden: false,
    snapshotCount: data.length,
    lastAccessed: now - 10000,
    imageUrl: getPageThumbURL(TEST_URL1),
    imagePageUrl: TEST_URL1,
  });

  info("add a featured image for second oldest snapshot");
  let previewImageURL = TEST_IMAGE_URL2;
  await PlacesUtils.history.update({
    url: TEST_URL2,
    previewImageURL,
  });
  groups = await SnapshotGroups.query({ skipMinimum: true });
  assertSnapshotGroup(groups[0], {
    title: "Test Group",
    builder: "domain",
    builderMetadata: { title: "Test Group" },
    hidden: false,
    snapshotCount: data.length,
    lastAccessed: now - 10000,
    imageUrl: previewImageURL,
    faviconDataUrl: FAVICON_DATAURL2,
    imagePageUrl: TEST_URL2,
  });

  info("add a featured image for the oldest snapshot");
  previewImageURL = TEST_IMAGE_URL1;
  await PlacesUtils.history.update({
    url: TEST_URL1,
    previewImageURL,
  });
  groups = await SnapshotGroups.query({ skipMinimum: true });
  assertSnapshotGroup(groups[0], {
    title: "Test Group",
    builder: "domain",
    builderMetadata: { title: "Test Group" },
    hidden: false,
    snapshotCount: data.length,
    lastAccessed: now - 10000,
    imageUrl: previewImageURL,
    faviconDataUrl: FAVICON_DATAURL1,
    imagePageUrl: TEST_URL1,
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
    hidden: false,
    builderMetadata: { title: "Test Group", domain: "example.com" },
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
    builderMetadata: { title: "Test Group", domain: "example.com" },
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
  // This should be ignored.
  groups[0].builder = "pinned";
  await SnapshotGroups.updateMetadata(groups[0]);

  let updated_groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(updated_groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(updated_groups[0], {
    title: "Modified title",
    builder: "domain",
    builderMetadata: { domain: "example.com", title: "Test Group" },
    snapshotCount: [TEST_URL3, TEST_URL2, TEST_URL1].length,
  });

  await SnapshotGroups.updateMetadata({
    id: groups[0].id,
    title: "Only changed title",
  });

  updated_groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(updated_groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(updated_groups[0], {
    title: "Only changed title",
    builder: "domain",
    builderMetadata: { domain: "example.com", title: "Test Group" },
    snapshotCount: [TEST_URL3, TEST_URL2, TEST_URL1].length,
  });

  await SnapshotGroups.updateMetadata({
    id: groups[0].id,
    builderMetadata: { foo: "bar" },
  });

  updated_groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(updated_groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(updated_groups[0], {
    title: "Only changed title",
    builder: "domain",
    builderMetadata: {
      domain: "example.com",
      title: "Test Group",
      foo: "bar",
    },
    snapshotCount: [TEST_URL3, TEST_URL2, TEST_URL1].length,
  });

  await SnapshotGroups.updateMetadata({
    id: groups[0].id,
    title: "Modified title",
  });

  updated_groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(updated_groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(updated_groups[0], {
    title: "Modified title",
    builder: "domain",
    builderMetadata: { domain: "example.com", title: "Test Group", foo: "bar" },
    snapshotCount: [TEST_URL3, TEST_URL2, TEST_URL1].length,
  });

  info("Restore the original snapshot group title");
  await SnapshotGroups.updateMetadata({
    id: groups[0].id,
    title: null,
  });

  updated_groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(updated_groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(updated_groups[0], {
    title: "Test Group",
    builder: "domain",
    builderMetadata: { domain: "example.com", title: "Test Group", foo: "bar" },
    snapshotCount: [TEST_URL3, TEST_URL2, TEST_URL1].length,
  });
});

add_task(async function test_update_urls() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 snapshot group");
  assertSnapshotGroup(groups[0], {
    title: "Test Group",
    builder: "domain",
    builderMetadata: { domain: "example.com", title: "Test Group", foo: "bar" },
    snapshotCount: [TEST_URL3, TEST_URL2, TEST_URL1].length,
  });

  await SnapshotGroups.updateUrls(groups[0].id, [
    TEST_URL5,
    TEST_URL3,
    TEST_URL1,
  ]);

  let updated_groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(updated_groups.length, 1, "Should return 1 SnapshotGroup");
  assertSnapshotGroup(groups[0], {
    title: "Test Group",
    builder: "domain",
    builderMetadata: { domain: "example.com", title: "Test Group", foo: "bar" },
    snapshotCount: [TEST_URL5, TEST_URL3, TEST_URL1].length,
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
    builderMetadata: { title: "Second Group" },
    snapshotCount: 1,
  });
  assertSnapshotGroup(groups[1], {
    title: "First Group",
    builder: "domain",
    builderMetadata: { title: "First Group" },
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
    builderMetadata: { title: "No url group" },
    snapshotCount: 0,
  });
});

add_task(async function test_get_urls() {
  await delete_all_groups();

  let urls = [TEST_URL1, TEST_URL2, TEST_URL3];
  await addInteractionsAndSnapshots(urls);

  let newGroup = { title: "Test Group", builder: "domain" };
  await SnapshotGroups.add(newGroup, urls);

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  let snapshots = await SnapshotGroups.getUrls({ id: groups[0].id });
  Assert.deepEqual(
    snapshots,
    [TEST_URL3, TEST_URL2, TEST_URL1],
    "Should return 3 urls"
  );
});

add_task(async function test_get_urls() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  await SnapshotGroups.setUrlHidden(groups[0].id, TEST_URL2, true);

  let snapshots = await SnapshotGroups.getUrls({ id: groups[0].id });
  Assert.deepEqual(snapshots, [TEST_URL3, TEST_URL1], "Should return 2 urls");

  snapshots = await SnapshotGroups.getUrls({ id: groups[0].id, hidden: true });
  Assert.deepEqual(
    snapshots,
    [TEST_URL3, TEST_URL2, TEST_URL1],
    "Should return all urls"
  );
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

add_task(async function test_get_snapshots_sortDescending() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  let snapshots = await SnapshotGroups.getSnapshots({
    id: groups[0].id,
    sortDescending: false,
  });
  await assertSnapshotList(snapshots, [
    { url: TEST_URL1 },
    { url: TEST_URL2 },
    { url: TEST_URL3 },
  ]);
});

add_task(async function test_get_snapshots_sortBy() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  let snapshots = await SnapshotGroups.getSnapshots({
    id: groups[0].id,
    sortBy: "last_interaction_at",
  });
  await assertSnapshotList(snapshots, [
    { url: TEST_URL3 },
    { url: TEST_URL2 },
    { url: TEST_URL1 },
  ]);
});

add_task(async function test_get_snapshots_hidden() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");
  Assert.equal(
    groups[0].snapshotCount,
    3,
    "Should have the correct count of snapshots"
  );

  await SnapshotGroups.setUrlHidden(groups[0].id, TEST_URL2, true);

  groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(
    groups[0].snapshotCount,
    2,
    "Should have the correct count of snapshots when not counting hidden ones"
  );

  groups = await SnapshotGroups.query({ skipMinimum: true, countHidden: true });
  Assert.equal(
    groups[0].snapshotCount,
    3,
    "Should have the correct count of snapshots when counting hidden ones"
  );

  let snapshots = await SnapshotGroups.getSnapshots({
    id: groups[0].id,
    sortBy: "last_interaction_at",
  });
  await assertSnapshotList(snapshots, [{ url: TEST_URL3 }, { url: TEST_URL1 }]);

  snapshots = await SnapshotGroups.getSnapshots({
    id: groups[0].id,
    hidden: true,
    sortBy: "last_interaction_at",
  });

  await assertSnapshotList(snapshots, [
    { url: TEST_URL3 },
    { url: TEST_URL2 },
    { url: TEST_URL1 },
  ]);

  await SnapshotGroups.setUrlHidden(groups[0].id, TEST_URL2, false);

  groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(
    groups[0].snapshotCount,
    3,
    "Should have the correct count of snapshots when not counting hidden ones"
  );

  snapshots = await SnapshotGroups.getSnapshots({
    id: groups[0].id,
    sortBy: "last_interaction_at",
  });

  await assertSnapshotList(snapshots, [
    { url: TEST_URL3 },
    { url: TEST_URL2 },
    { url: TEST_URL1 },
  ]);
});

add_task(async function test_snapshot_image_with_hidden() {
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  // The images were set on TEST_URL1/TEST_URL2 in an earlier test.
  assertSnapshotGroup(groups[0], {
    imageUrl: TEST_IMAGE_URL1,
    imagePageUrl: TEST_URL1,
  });

  await SnapshotGroups.setUrlHidden(groups[0].id, TEST_URL1, true);

  groups = await SnapshotGroups.query({ skipMinimum: true });
  info("Should use the oldest non-hidden image");
  assertSnapshotGroup(groups[0], {
    imageUrl: TEST_IMAGE_URL2,
    imagePageUrl: TEST_URL2,
  });
});

add_task(async function test_minimum_size() {
  let urls = [TEST_URL1, TEST_URL2, TEST_URL3];
  let groupId = await SnapshotGroups.add(
    { title: "Test Group 2", builder: "domain" },
    urls
  );

  let pinnedUrls = [];
  let pinnedGroupId = await SnapshotGroups.add(
    { title: "Test Pinned", builder: "pinned" },
    pinnedUrls
  );

  let groups = await SnapshotGroups.query();
  Assert.equal(
    groups.length,
    0,
    "Should not have returned groups under the limit nor fixed groups with no items"
  );

  // The pinned group is a special builder that should always have the group
  // returned if it exists.
  pinnedUrls.push(TEST_PINNED_URL);
  await addInteractionsAndSnapshots([
    { url: TEST_PINNED_URL, userPersisted: Snapshots.USER_PERSISTED.PINNED },
  ]);
  await SnapshotGroups.updateUrls(pinnedGroupId, pinnedUrls);

  groups = await SnapshotGroups.query();
  Assert.equal(
    groups.length,
    1,
    "Should have returned the pinned group and no other groups under the limit"
  );
  assertSnapshotGroup(groups[0], {
    title: "Test Pinned",
    builder: "pinned",
    snapshotCount: 1,
  });

  urls.push(TEST_URL4);
  await SnapshotGroups.updateUrls(groupId, urls);

  groups = await SnapshotGroups.query();
  Assert.equal(
    groups.length,
    2,
    "Should have returned both groups now they are over the limit"
  );

  // Ensure the results are in a consistent order.
  groups.sort((a, b) => a.title.localeCompare(b.title));

  assertSnapshotGroup(groups[0], {
    title: "Test Group 2",
    builder: "domain",
    snapshotCount: 4,
  });
  assertSnapshotGroup(groups[1], {
    title: "Test Pinned",
    builder: "pinned",
    snapshotCount: 1,
  });
});

add_task(async function test_hidden_groups() {
  await delete_all_groups();

  let group1 = await SnapshotGroups.add(
    {
      title: "Test Group 1",
      builder: "domain",
    },
    []
  );

  let group2 = await SnapshotGroups.add(
    {
      title: "Test Group 2",
      builder: "domain",
    },
    []
  );

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 2, "Should return all groups.");
  assertSnapshotGroupList(orderedGroups(groups, [group1, group2]), [
    {
      title: "Test Group 1",
      builder: "domain",
      hidden: false,
      snapshotCount: 0,
    },
    {
      title: "Test Group 2",
      builder: "domain",
      hidden: false,
      snapshotCount: 0,
    },
  ]);

  await SnapshotGroups.updateMetadata({
    id: group1,
    hidden: true,
  });

  groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should be only one visible group.");
  assertSnapshotGroup(groups[0], {
    title: "Test Group 2",
    builder: "domain",
    hidden: false,
    snapshotCount: 0,
  });

  groups = await SnapshotGroups.query({ hidden: true, skipMinimum: true });
  Assert.equal(groups.length, 2, "Should be two total groups.");
  assertSnapshotGroupList(orderedGroups(groups, [group1, group2]), [
    {
      title: "Test Group 1",
      builder: "domain",
      hidden: true,
      snapshotCount: 0,
    },
    {
      title: "Test Group 2",
      builder: "domain",
      hidden: false,
      snapshotCount: 0,
    },
  ]);

  await SnapshotGroups.updateMetadata({
    id: group1,
    hidden: false,
  });

  groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 2, "Should be two visible groups.");
  assertSnapshotGroupList(orderedGroups(groups, [group1, group2]), [
    {
      title: "Test Group 1",
      builder: "domain",
      hidden: false,
      snapshotCount: 0,
    },
    {
      title: "Test Group 2",
      builder: "domain",
      hidden: false,
      snapshotCount: 0,
    },
  ]);

  groups = await SnapshotGroups.query({ hidden: true, skipMinimum: true });
  Assert.equal(groups.length, 2, "Should be two total groups.");

  await SnapshotGroups.updateMetadata({
    id: group2,
    hidden: true,
  });

  groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should be only one visible group.");
  assertSnapshotGroup(groups[0], {
    title: "Test Group 1",
    builder: "domain",
    hidden: false,
    snapshotCount: 0,
  });

  groups = await SnapshotGroups.query({ hidden: true, skipMinimum: true });
  Assert.equal(groups.length, 2, "Should be two total groups.");
  assertSnapshotGroupList(orderedGroups(groups, [group1, group2]), [
    {
      title: "Test Group 1",
      builder: "domain",
      hidden: false,
      snapshotCount: 0,
    },
    {
      title: "Test Group 2",
      builder: "domain",
      hidden: true,
      snapshotCount: 0,
    },
  ]);
});

add_task(async function test_snapshots_remain_hidden_after_updateUrls() {
  await delete_all_groups();

  await SnapshotGroups.add(
    {
      title: "Test Group 1",
      builder: "domain",
    },
    [TEST_URL1, TEST_URL2, TEST_URL3]
  );

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1, "Should return 1 SnapshotGroup");

  await SnapshotGroups.setUrlHidden(groups[0].id, TEST_URL2, true);

  let snapshots = await SnapshotGroups.getSnapshots({
    id: groups[0].id,
    sortBy: "last_interaction_at",
  });
  await assertSnapshotList(snapshots, [{ url: TEST_URL3 }, { url: TEST_URL1 }]);

  await SnapshotGroups.updateUrls(groups[0].id, [
    TEST_URL1,
    TEST_URL2,
    TEST_URL3,
    TEST_URL4,
  ]);

  snapshots = await SnapshotGroups.getSnapshots({
    id: groups[0].id,
    sortBy: "last_interaction_at",
  });
  await assertSnapshotList(snapshots, [
    { url: TEST_URL3 },
    { url: TEST_URL1 },
    { url: TEST_URL4 },
  ]);
});

add_task(async function test_similar_snapshot_titles_difference_indication() {
  await delete_all_groups();

  let group1 = await SnapshotGroups.add(
    {
      title: "Test Group 1",
      builder: "domain",
    },
    [TEST_URL1, TEST_URL2]
  );
  let snapshots = await SnapshotGroups.getSnapshots({
    id: group1,
    sortBy: "last_interaction_at",
  });

  await assertSnapshotList(snapshots, [
    { url: TEST_URL2, titleDifferentIndex: 35 },
    { url: TEST_URL1, titleDifferentIndex: 35 },
  ]);
});
