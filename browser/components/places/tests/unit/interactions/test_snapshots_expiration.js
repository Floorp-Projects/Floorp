/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for the expiration of snapshots.
 */

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SNAPSHOT_EXPIRE_DAYS",
  "browser.places.snapshots.expiration.days",
  210
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "SNAPSHOT_USERMANAGED_EXPIRE_DAYS",
  "browser.places.snapshots.expiration.userManaged.days",
  420
);

const now = Date.now();
const MS_PER_DAY = 86400000;
let groupSerial = 0;

// For each snapshot define:
// {
//   url: url of the snapshot.
//   [userPersisted]: optional userPersisted value.
//   created_at: time of the last interaction.
//   [tombstone]: whether it should be a tombstone.
//   [group]: name of a manual group to be created for this snapshot.
//   shouldExpire: whether we expect the snapshot to expire.
// }
let gSnapshots = [
  {
    // User persisted snapshot.
    // Expired because time interacted before USERMANAGED time.
    url: "https://example.com/1",
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    created_at: now - (1 + SNAPSHOT_USERMANAGED_EXPIRE_DAYS) * MS_PER_DAY,
    shouldExpire: true,
  },
  {
    // User persisted snapshot.
    // Not expired because interacted after USERMANAGED time.
    url: "https://example.com/2",
    userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    created_at: now - (1 + SNAPSHOT_EXPIRE_DAYS) * MS_PER_DAY,
    shouldExpire: false,
  },
  {
    // Automatic snapshot in a user managed group.
    // Expired because interacted before USERMANAGED time.
    url: "https://example.com/3",
    created_at: now - (1 + SNAPSHOT_USERMANAGED_EXPIRE_DAYS) * MS_PER_DAY,
    group: `test-group-${groupSerial++}`,
    shouldExpire: true,
  },
  {
    // Automatic snapshot in a user managed group.
    // Not expired because interacted after USERMANAGED time.
    url: "https://example.com/4",
    created_at: now - (1 + SNAPSHOT_EXPIRE_DAYS) * MS_PER_DAY,
    group: `test-group-${groupSerial++}`,
    shouldExpire: false,
  },
  {
    // Automatic snapshot.
    // Expired because interacted after USERMANAGED time.
    url: "https://example.com/5",
    created_at: now - (1 + SNAPSHOT_EXPIRE_DAYS) * MS_PER_DAY,
    shouldExpire: true,
  },
  {
    // Automatic snapshot.
    // Not expired because interacted very recently.
    url: "https://example.com/6",
    created_at: now - MS_PER_DAY,
    shouldExpire: false,
  },
  {
    // Automatic snapshot.
    // Not expired because it's a tombstone.
    url: "https://example.com/7",
    created_at: now - (1 + SNAPSHOT_USERMANAGED_EXPIRE_DAYS) * MS_PER_DAY,
    tombstone: true,
    shouldExpire: false,
  },
];

add_task(async function setup() {
  await addInteractions(gSnapshots);

  for (let snapshot of gSnapshots) {
    await Snapshots.add(snapshot);
    if (snapshot.group) {
      await SnapshotGroups.add(
        {
          title: snapshot.group,
          builder: "user",
        },
        [snapshot.url]
      );
    }
    if (snapshot.tombstone) {
      await Snapshots.delete(snapshot.url);
    }
  }

  Services.prefs.setBoolPref("browser.places.interactions.enabled", true);

  // This ensures that all the snapshots are in an automatic group, as it would
  // actually be in reality, since the domain group builder picks up most
  // sites. This means all the expiration tests happen within groups.
  // Note that snapshots having a manual group will also be part of the domain
  // automatic group, and that's important for this test, because we must check
  // that even if they are part of different kind of groups, the expiration
  // considers the stricter user managed ones.
  await SnapshotMonitor.observe(null, "test-trigger-builders");
  let groups = await SnapshotGroups.query({ skipMinimum: true });
  Assert.equal(
    groups.length,
    1 + groupSerial,
    "Should return the expected number of snapshot groups"
  );
  assertSnapshotGroup(groups[0], {
    title: "example",
    builder: "domain",
    builderMetadata: { domain: "example.com", title: "example" },
    snapshotCount: gSnapshots.filter(s => !s.tombstone).length,
  });
});

add_task(async function test_idle_expiration() {
  await SnapshotMonitor.observe({ onIdle: true }, "test-expiration");

  let remaining = await Snapshots.query({ includeTombstones: true });
  for (let snapshot of gSnapshots) {
    let index = remaining.findIndex(s => s.url == snapshot.url);
    if (snapshot.shouldExpire) {
      Assert.equal(index, -1, `${snapshot.url} should have been removed`);
    } else {
      Assert.greater(index, -1, `${snapshot.url} should not have been removed`);
      remaining.splice(index, 1);
    }
  }
  Assert.ok(
    !remaining.length,
    `All the snapshots should be processed: ${JSON.stringify(remaining)}`
  );
});

add_task(async function test_active_limited_expiration() {
  // Add 2 expirable snapshots.
  let expiredSnapshots = [
    {
      url: "https://example.com/8",
      created_at: now - (SNAPSHOT_USERMANAGED_EXPIRE_DAYS + 1) * 86400000,
    },
    {
      url: "https://example.com/9",
      created_at: now - (SNAPSHOT_USERMANAGED_EXPIRE_DAYS + 1) * 86400000,
    },
  ];
  for (let snapshot of expiredSnapshots) {
    await addInteractions([snapshot]);
    await Snapshots.add(snapshot);
  }

  let snapshots = await Snapshots.query({ includeTombstones: true });

  info("expire again without setting lastExpirationTime, should be a no-op");
  let expirationChunkSize = 1;
  await SnapshotMonitor.observe(
    {
      expirationChunkSize,
    },
    "test-expiration"
  );

  // Since expiration just ran, nothing should have been expired.
  Assert.equal(
    (await Snapshots.query({ includeTombstones: true })).length,
    snapshots.length,
    "No snapshot should have been expired."
  );

  info("expire again, for real");
  await SnapshotMonitor.observe(
    {
      expirationChunkSize,
      lastExpirationTime: now - 24 * 86400000,
    },
    "test-expiration"
  );

  let remaining = await Snapshots.query({ includeTombstones: true });

  let count = 0;
  for (let snapshot of expiredSnapshots) {
    let index = remaining.findIndex(s => s.url == snapshot.url);
    if (index == -1) {
      count++;
    }
  }
  Assert.equal(
    count,
    expiredSnapshots.length - expirationChunkSize,
    "Check the expected number of snapshots have been expired"
  );
});
