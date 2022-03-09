/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for the SnapshotMonitor to check that it triggers builders at the
 * correct times.
 */

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const TEST_URLS = [
  "https://example.com/1",
  "https://example.com/2",
  "https://example.com/3",
  "https://example.com/4",
  "https://example.com/5",
];

const fakeBuilder = {
  rebuild: sinon.stub(),
  update: sinon.stub(),
};

add_task(async function setup() {
  await addInteractions(
    TEST_URLS.map(url => {
      return { url };
    })
  );

  Services.prefs.setBoolPref("browser.places.interactions.enabled", true);

  SnapshotMonitor.init();
  SnapshotMonitor.testGroupBuilders = [fakeBuilder];
});

add_task(async function test_idle_daily() {
  sinon.reset();

  await SnapshotMonitor.observe(null, "idle-daily");

  Assert.ok(
    fakeBuilder.rebuild.calledOnce,
    "Should have triggered a full rebuild"
  );
  Assert.ok(
    !fakeBuilder.update.called,
    "Should not have triggered an intermediate update"
  );
});

function assertUpdateCalled({ addedItems = [], removedUrls = [] }) {
  Assert.ok(
    fakeBuilder.rebuild.notCalled,
    "Should not have triggered a rebuild"
  );
  Assert.ok(
    fakeBuilder.update.calledOnce,
    "Should have triggered an intermediate update"
  );
  let arg = fakeBuilder.update.args[0][0];

  Assert.deepEqual(
    [...arg.addedItems.values()],
    addedItems,
    "Should have added the expected urls"
  );
  Assert.deepEqual(
    [...arg.removedUrls.values()],
    removedUrls,
    "Should have removed the expected urls"
  );
}

add_task(async function test_add_single_snapshot() {
  sinon.reset();

  let time = Date.now();
  await new Promise(resolve => {
    SnapshotMonitor.setTimerDelaysForTests({ added: 50, removed: 10000 });
    fakeBuilder.update.callsFake(resolve);

    Snapshots.add({ url: TEST_URLS[0] });
  });

  Assert.less(Date.now() - time, 300);

  assertUpdateCalled({
    addedItems: [
      { url: TEST_URLS[0], userPersisted: Snapshots.USER_PERSISTED.NO },
    ],
  });
});

add_task(async function test_add_multiple_snapshot() {
  sinon.reset();

  let time = Date.now();
  await new Promise(resolve => {
    SnapshotMonitor.setTimerDelaysForTests({ added: 50, removed: 10000 });
    fakeBuilder.update.callsFake(resolve);

    Snapshots.add({ url: TEST_URLS[1] });
    Snapshots.add({ url: TEST_URLS[2] });
  });

  Assert.less(Date.now() - time, 300);

  assertUpdateCalled({
    addedItems: [
      { url: TEST_URLS[1], userPersisted: Snapshots.USER_PERSISTED.NO },
      { url: TEST_URLS[2], userPersisted: Snapshots.USER_PERSISTED.NO },
    ],
  });
});

add_task(async function test_remove_snapshots() {
  sinon.reset();

  let time = Date.now();
  await new Promise(resolve => {
    SnapshotMonitor.setTimerDelaysForTests({ added: 10000, removed: 50 });
    fakeBuilder.update.callsFake(resolve);

    Snapshots.delete(TEST_URLS[1]);
    Snapshots.delete(TEST_URLS[2]);
  });

  Assert.less(Date.now() - time, 300);

  assertUpdateCalled({ removedUrls: [TEST_URLS[1], TEST_URLS[2]] });
});

add_task(async function test_add_and_remove_snapshots() {
  sinon.reset();

  let time = Date.now();
  await new Promise(resolve => {
    SnapshotMonitor.setTimerDelaysForTests({ added: 10000, removed: 50 });
    fakeBuilder.update.callsFake(resolve);

    Snapshots.add({ url: TEST_URLS[3] });
    Snapshots.add({ url: TEST_URLS[4] });
    Snapshots.delete(TEST_URLS[0]);
  });

  Assert.less(Date.now() - time, 300);

  assertUpdateCalled({
    addedItems: [
      { url: TEST_URLS[3], userPersisted: Snapshots.USER_PERSISTED.NO },
      { url: TEST_URLS[4], userPersisted: Snapshots.USER_PERSISTED.NO },
    ],
    removedUrls: [TEST_URLS[0]],
  });
});
