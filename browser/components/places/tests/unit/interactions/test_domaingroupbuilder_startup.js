/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for ensuring the domain builder works correctly when starting with an
 * incremental update.
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
  for (let url of TEST_URLS) {
    await Snapshots.add({ url });
  }
  let groupId = await SnapshotGroups.add(
    {
      title: "example",
      builder: "domain",
      builderMetadata: {
        domain: "example.com",
      },
    },
    TEST_URLS
  );

  await SnapshotGroups.setUrlHidden(groupId, TEST_URLS[2], true);
});

add_task(async function test_update_correctly_updates_group() {
  await Snapshots.add({ url: TEST_URLS_EXTRA });

  await DomainGroupBuilder.update({
    addedItems: new Set([
      { url: TEST_URLS_EXTRA, userPersisted: Snapshots.USER_PERSISTED.NO },
    ]),
    removedUrls: new Set(),
  });

  let groups = await SnapshotGroups.query();

  Assert.equal(groups.length, 1);
  assertSnapshotGroup(groups[0], {
    title: "example",
    builder: "domain",
    builderMetadata: { title: "example", domain: "example.com" },
  });

  let urls = await SnapshotGroups.getUrls({ id: groups[0].id, hidden: true });
  Assert.deepEqual(
    urls.sort(),
    [...TEST_URLS, TEST_URLS_EXTRA].sort(),
    "Should have inserted the expected URLs"
  );
});
