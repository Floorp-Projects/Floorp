/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for ensuring the domain builder works correctly.
 */

ChromeUtils.defineModuleGetter(
  this,
  "DomainGroupBuilder",
  "resource:///modules/DomainGroupBuilder.jsm"
);

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
      addedUrls: new Set(TEST_URLS),
      removedUrls: new Set(),
    });
  }

  await groupAdded;

  let groups = await SnapshotGroups.query();

  Assert.equal(groups.length, 1);
  assertSnapshotGroup(groups[0], {
    title: "example",
    builder: "domain",
    builderMetadata: { domain: "example.com" },
    urls: TEST_URLS,
  });
}

async function modifyGroupTest(shouldRebuild) {
  let groupUpdated = TestUtils.topicObserved("places-snapshot-group-updated");

  await Snapshots.add({ url: TEST_URLS_EXTRA });

  if (shouldRebuild) {
    let snapshots = await Snapshots.query({ limit: -1 });
    await DomainGroupBuilder.rebuild(snapshots);
  } else {
    await DomainGroupBuilder.update({
      addedUrls: new Set([TEST_URLS_EXTRA]),
      removedUrls: new Set(),
    });
  }

  await groupUpdated;

  let groups = await SnapshotGroups.query();

  Assert.equal(groups.length, 1);
  assertSnapshotGroup(groups[0], {
    title: "example",
    builder: "domain",
    builderMetadata: { domain: "example.com" },
    // TODO: Replace when updateUrls API has been implemented.
    urls: TEST_URLS,
    // urls: [...TEST_URLS, TEST_URLS_EXTRA],
  });
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
      addedUrls: new Set(),
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
