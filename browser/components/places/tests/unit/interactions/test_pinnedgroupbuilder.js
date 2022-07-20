/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for ensuring the domain builder works correctly.
 */

ChromeUtils.defineESModuleGetters(this, {
  PinnedGroupBuilder: "resource:///modules/PinnedGroupBuilder.sys.mjs",
});

const TEST_PERSISTED_URLS = ["https://example.com/1", "https://example.com/2"];
const TEST_EXTRA_PERSISTED_URL = "https://example.com/3";
const TEST_NON_PERSISTED_URL1 = "https://example.com/4";
const TEST_NON_PERSISTED_URL2 = "https://example.com/4";

add_task(async function setup() {
  await addInteractions(
    [
      ...TEST_PERSISTED_URLS,
      TEST_EXTRA_PERSISTED_URL,
      TEST_NON_PERSISTED_URL1,
      TEST_NON_PERSISTED_URL2,
    ].map(url => {
      return {
        url,
      };
    })
  );
});

async function addGroupTest(shouldRebuild) {
  let groupAdded = TestUtils.topicObserved("places-snapshot-group-added");

  for (let url of TEST_PERSISTED_URLS) {
    await Snapshots.add({
      url,
      userPersisted: Snapshots.USER_PERSISTED.PINNED,
    });
  }
  await Snapshots.add({ url: TEST_NON_PERSISTED_URL1 });

  if (shouldRebuild) {
    let snapshots = await Snapshots.query({ limit: -1 });
    await PinnedGroupBuilder.rebuild(snapshots);
  } else {
    await PinnedGroupBuilder.update({
      addedItems: new Set([
        ...TEST_PERSISTED_URLS.map(url => {
          return { url, userPersisted: Snapshots.USER_PERSISTED.PINNED };
        }),
        {
          url: TEST_NON_PERSISTED_URL1,
          userPersisted: Snapshots.USER_PERSISTED.NO,
        },
      ]),
      removedUrls: new Set(),
    });
  }

  await groupAdded;

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1);
  assertSnapshotGroup(groups[0], {
    title: "",
    builder: "pinned",
    builderMetadata: {
      fluentTitle: { id: "snapshot-group-pinned-header" },
    },
  });

  let urls = await SnapshotGroups.getUrls({ id: groups[0].id });
  Assert.deepEqual(
    urls.sort(),
    TEST_PERSISTED_URLS.sort(),
    "Should have inserted the expected URLs"
  );
}

async function modifyGroupTest(shouldRebuild) {
  let groupUpdated = TestUtils.topicObserved("places-snapshot-group-updated");

  await Snapshots.add({
    url: TEST_EXTRA_PERSISTED_URL,
    userPersisted: Snapshots.USER_PERSISTED.PINNED,
  });
  await Snapshots.add({ url: TEST_NON_PERSISTED_URL2 });

  if (shouldRebuild) {
    let snapshots = await Snapshots.query({ limit: -1 });
    await PinnedGroupBuilder.rebuild(snapshots);
  } else {
    await PinnedGroupBuilder.update({
      addedItems: new Set([
        {
          url: TEST_EXTRA_PERSISTED_URL,
          userPersisted: Snapshots.USER_PERSISTED.PINNED,
        },
        {
          url: TEST_NON_PERSISTED_URL2,
          userPersisted: Snapshots.USER_PERSISTED.NO,
        },
      ]),
      removedUrls: new Set(),
    });
  }

  await groupUpdated;

  let groups = await SnapshotGroups.query({ skipMinimum: true });

  Assert.equal(groups.length, 1);
  assertSnapshotGroup(groups[0], {
    title: "",
    builder: "pinned",
    builderMetadata: {
      fluentTitle: { id: "snapshot-group-pinned-header" },
    },
  });
  let urls = await SnapshotGroups.getUrls({ id: groups[0].id });
  Assert.deepEqual(
    urls.sort(),
    [...TEST_PERSISTED_URLS, TEST_EXTRA_PERSISTED_URL].sort(),
    "Should have inserted the expected URLs"
  );
}

async function emptyGroupTest(shouldRebuild) {
  let groupUpdated = TestUtils.topicObserved("places-snapshot-group-updated");

  for (let url of [...TEST_PERSISTED_URLS, TEST_EXTRA_PERSISTED_URL]) {
    await Snapshots.delete(url);
  }

  if (shouldRebuild) {
    let snapshots = await Snapshots.query({ limit: -1 });
    await PinnedGroupBuilder.rebuild(snapshots);
  } else {
    await PinnedGroupBuilder.update({
      addedItems: new Set(),
      removedUrls: new Set([...TEST_PERSISTED_URLS, TEST_EXTRA_PERSISTED_URL]),
    });
  }

  await groupUpdated;

  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(groups.length, 1);
  assertSnapshotGroup(groups[0], {
    title: "",
    builder: "pinned",
    builderMetadata: {
      fluentTitle: { id: "snapshot-group-pinned-header" },
    },
  });
  let urls = await SnapshotGroups.getUrls({ id: groups[0].id });
  Assert.deepEqual(urls.length, 0, "Should not have any urls in the group");
}

add_task(async function test_rebuild_adds_group() {
  await addGroupTest(true);
});

add_task(async function test_rebuild_modifies_group() {
  await modifyGroupTest(true);
});

add_task(async function test_rebuild_empties_group() {
  await emptyGroupTest(true);
});

add_task(async function test_update_adds_group() {
  await Snapshots.reset();
  await PinnedGroupBuilder.reset();

  await addGroupTest(false);
});

add_task(async function test_update_modifies_group() {
  await modifyGroupTest(false);
});

add_task(async function test_update_empties_group() {
  await emptyGroupTest(false);
});
