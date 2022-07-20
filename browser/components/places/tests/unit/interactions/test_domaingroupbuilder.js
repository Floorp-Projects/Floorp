/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for ensuring the domain builder works correctly.
 */

ChromeUtils.defineESModuleGetters(this, {
  DomainGroupBuilder: "resource:///modules/DomainGroupBuilder.sys.mjs",
});

const TEST_URLS = [
  "https://example.com/1",
  "https://example.com/2",
  "https://example.com/3",
  "https://example.com/4",
  "https://example.com/5",
];
const TEST_URLS_EXTRA = "https://example.com/6";

add_task(async function setup() {
  await addInteractions(
    [...TEST_URLS, TEST_URLS_EXTRA].map(url => {
      return {
        url,
      };
    })
  );
});

async function addGroupTest(shouldRebuild) {
  let groupAdded = TestUtils.topicObserved("places-snapshot-group-added");

  for (let url of TEST_URLS) {
    await Snapshots.add({ url });
  }

  if (shouldRebuild) {
    let snapshots = await Snapshots.query({ limit: -1 });
    await DomainGroupBuilder.rebuild(snapshots);
  } else {
    await DomainGroupBuilder.update({
      addedItems: new Set(
        TEST_URLS.map(url => {
          return {
            url,
          };
        })
      ),
      removedUrls: new Set(),
    });
  }

  await groupAdded;

  let groups = await SnapshotGroups.query();

  Assert.equal(groups.length, 1);
  assertSnapshotGroup(groups[0], {
    title: "example",
    builder: "domain",
    builderMetadata: { title: "example", domain: "example.com" },
  });

  let urls = await SnapshotGroups.getUrls({ id: groups[0].id });
  Assert.deepEqual(
    urls.sort(),
    TEST_URLS.sort(),
    "Should have inserted the expected URLs"
  );
}

async function modifyGroupTest(shouldRebuild) {
  let groupUpdated = TestUtils.topicObserved("places-snapshot-group-updated");

  await Snapshots.add({ url: TEST_URLS_EXTRA });

  if (shouldRebuild) {
    let snapshots = await Snapshots.query({ limit: -1 });
    await DomainGroupBuilder.rebuild(snapshots);
  } else {
    await DomainGroupBuilder.update({
      addedItems: new Set([{ url: TEST_URLS_EXTRA }]),
      removedUrls: new Set(),
    });
  }

  await groupUpdated;

  let groups = await SnapshotGroups.query();

  Assert.equal(groups.length, 1);
  assertSnapshotGroup(groups[0], {
    title: "example",
    builder: "domain",
    builderMetadata: { title: "example", domain: "example.com" },
  });
  let urls = await SnapshotGroups.getUrls({ id: groups[0].id });
  Assert.deepEqual(
    urls.sort(),
    [...TEST_URLS, TEST_URLS_EXTRA].sort(),
    "Should have inserted the expected URLs"
  );
}

async function deleteGroupTest(shouldRebuild) {
  let groupDeleted = TestUtils.topicObserved("places-snapshot-group-deleted");

  for (let url of [TEST_URLS_EXTRA, ...TEST_URLS]) {
    await Snapshots.delete(url);
  }

  if (shouldRebuild) {
    let snapshots = await Snapshots.query({ limit: -1 });
    await DomainGroupBuilder.rebuild(snapshots);
  } else {
    await DomainGroupBuilder.update({
      addedItems: new Set(),
      removedUrls: new Set([TEST_URLS_EXTRA, ...TEST_URLS]),
    });
  }

  await groupDeleted;

  let groups = await SnapshotGroups.query();

  Assert.equal(groups.length, 0);
}

add_task(async function test_rebuild_adds_group() {
  await addGroupTest(true);
});

add_task(async function test_rebuild_modifies_group() {
  await modifyGroupTest(true);
});

add_task(async function test_rebuild_deletes_group() {
  await deleteGroupTest(true);
});

add_task(async function test_update_adds_group() {
  await Snapshots.reset();
  await DomainGroupBuilder.rebuild([]);

  await addGroupTest(false);
});

add_task(async function test_update_modifies_group() {
  await modifyGroupTest(false);
});

add_task(async function test_update_deletes_group() {
  await deleteGroupTest(false);
});
