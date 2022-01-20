/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that Snapshots can only be created for an allow list of protocols.
 */

let tests = [
  {
    url: "https://example-foo.com/",
    canSnapshot: true,
  },
  {
    url: "http://example-foo/",
    canSnapshot: true,
  },
  {
    url: "file:///Users/joe/somefile.pdf",
    canSnapshot: true,
  },
  {
    url: "someprotocol://bar/",
    canSnapshot: false,
  },
  {
    url: "file:///Users/bill/",
    canSnapshot: false,
  },
];

add_task(async function test_add_snapshot() {
  for (let { url, canSnapshot } of tests) {
    await PlacesTestUtils.addVisits(url);
    if (canSnapshot) {
      await Snapshots.add({
        url,
        userPersisted: Snapshots.USER_PERSISTED.MANUAL,
      });
    } else {
      await Assert.rejects(
        Snapshots.add({ url, userPersisted: Snapshots.USER_PERSISTED.MANUAL }),
        /url cannot be added/,
        `Check ${url} cannot be added to snapshots`
      );
    }
  }
  Snapshots.reset();
});

add_task(async function test_add_snapshot() {
  let snapshots = [];
  for (let { url, canSnapshot } of tests) {
    await PlacesTestUtils.addVisits(url);
    await addInteractions([
      {
        url,
        totalViewTime: 40000,
      },
    ]);
    if (canSnapshot) {
      snapshots.push({ url });
    }
  }

  await assertSnapshots(snapshots.reverse());
});
