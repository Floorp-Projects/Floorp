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

// For each snapshot define its url, whether it should be expired, whether it
// should be a tomstone and a type.
// The type may be:
//   - manual: user_persisted
//   - auto: created automatically by some heuristic
//   - group: part of a group
let gSnapshots = [
  {
    url: "https://example.com/1",
    type: "manual",
    expired: true,
    tombstone: false,
  },
  {
    url: "https://example.com/2",
    type: "manual",
    expired: false,
    tombstone: false,
  },
  {
    url: "https://example.com/3",
    type: "group",
    expired: true,
    tombstone: false,
  },
  {
    url: "https://example.com/4",
    type: "auto",
    expired: true,
    tombstone: false,
  },
  {
    url: "https://example.com/5",
    type: "auto",
    expired: false,
    tombstone: false,
  },
  {
    url: "https://example.com/6",
    type: "auto",
    expired: true,
    tombstone: true,
  },
];

add_task(async function setup() {
  let now = Date.now();
  let interactions = gSnapshots.map(s => {
    if (s.expired) {
      s.created_at =
        now -
        (1 + s.type != "auto"
          ? SNAPSHOT_USERMANAGED_EXPIRE_DAYS
          : SNAPSHOT_EXPIRE_DAYS) *
          86400000;
    } else {
      s.created_at =
        now - (s.type != "auto" ? SNAPSHOT_EXPIRE_DAYS : 1) * 86400000;
    }
    return s;
  });
  await addInteractions(interactions);

  let groupSerial = 1;
  for (let snapshot of gSnapshots) {
    if (snapshot.type == "manual") {
      snapshot.userPersisted = Snapshots.USER_PERSISTED.MANUAL;
    }
    await Snapshots.add(snapshot);
    if (snapshot.type == "group") {
      snapshot.group = await SnapshotGroups.add(
        {
          title: `test-group-${groupSerial++}`,
          builder: "test",
        },
        [snapshot.url]
      );
    }
    if (snapshot.tombstone) {
      await Snapshots.delete(snapshot.url);
    }
  }

  Services.prefs.setBoolPref("browser.places.interactions.enabled", true);

  SnapshotMonitor.init();
});

add_task(async function test_idle_expiration() {
  await SnapshotMonitor.observe({ onIdle: true }, "test-expiration");

  let remaining = await Snapshots.query({ includeTombstones: true });
  for (let snapshot of gSnapshots) {
    let index = remaining.findIndex(s => s.url == snapshot.url);
    if (!snapshot.expired || snapshot.tombstone) {
      Assert.greater(index, -1, `${snapshot.url} should not have been removed`);
      remaining.splice(index, 1);
    } else {
      Assert.equal(index, -1, `${snapshot.url} should have been removed`);
    }
  }
  Assert.ok(
    !remaining.length,
    `All the snapshots should be processed: ${JSON.stringify(remaining)}`
  );
});

add_task(async function test_active_limited_expiration() {
  // Add 2 expirable snapshots.
  let now = Date.now();
  let expiredSnapshots = [
    {
      url: "https://example.com/7",
      created_at: now - (SNAPSHOT_USERMANAGED_EXPIRE_DAYS + 1) * 86400000,
    },
    {
      url: "https://example.com/8",
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
