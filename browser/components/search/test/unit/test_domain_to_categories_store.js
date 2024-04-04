/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Ensure that the domain to categories store public methods work as expected
 * and it handles all error cases as expected.
 */

ChromeUtils.defineESModuleGetters(this, {
  CATEGORIZATION_SETTINGS: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  DomainToCategoriesStore: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

let store = new DomainToCategoriesStore();
let defaultStorePath;
let fileContents = [convertToBuffer({ foo: [0, 1] })];

async function createCorruptedStore() {
  info("Create a corrupted store.");
  let storePath = PathUtils.join(
    PathUtils.profileDir,
    CATEGORIZATION_SETTINGS.STORE_FILE
  );
  let src = PathUtils.join(do_get_cwd().path, "corruptDB.sqlite");
  await IOUtils.copy(src, storePath);
  Assert.ok(await IOUtils.exists(storePath), "Store exists.");
  return storePath;
}

function convertToBuffer(obj) {
  return new TextEncoder().encode(JSON.stringify(obj)).buffer;
}

/**
 * Deletes data from the store and removes any files that were generated due
 * to them.
 */
async function cleanup() {
  info("Clean up store.");

  // In these tests, we sometimes use read-only files to test permission error
  // handling. On Windows, we have to change it to writable to allow for their
  // deletion so that subsequent tests aren't affected.
  if (
    (await IOUtils.exists(defaultStorePath)) &&
    Services.appinfo.OS == "WINNT"
  ) {
    await IOUtils.setPermissions(defaultStorePath, 0o600);
  }

  await store.testDelete();
  Assert.equal(store.empty, true, "Store should be empty.");
  Assert.equal(await IOUtils.exists(defaultStorePath), false, "Store exists.");
  Assert.equal(
    await store.getVersion(),
    0,
    "Version number should be 0 when store is empty."
  );

  await store.uninit();
}

async function createReadOnlyStore() {
  info("Create a store that can't be read.");
  let storePath = PathUtils.join(
    PathUtils.profileDir,
    CATEGORIZATION_SETTINGS.STORE_FILE
  );

  let conn = await Sqlite.openConnection({ path: storePath });
  await conn.execute("CREATE TABLE test (id INTEGER PRIMARY KEY)");
  await conn.close();

  await changeStoreToReadOnly();
}

async function changeStoreToReadOnly() {
  info("Change store to read only.");
  let storePath = PathUtils.join(
    PathUtils.profileDir,
    CATEGORIZATION_SETTINGS.STORE_FILE
  );
  let stat = await IOUtils.stat(storePath);
  await IOUtils.setPermissions(storePath, 0o444);
  stat = await IOUtils.stat(storePath);
  Assert.equal(stat.permissions, 0o444, "Permissions should be read only.");
  Assert.ok(await IOUtils.exists(storePath), "Store exists.");
}

add_setup(async function () {
  // We need a profile directory to create the store and open a connection.
  do_get_profile();
  defaultStorePath = PathUtils.join(
    PathUtils.profileDir,
    CATEGORIZATION_SETTINGS.STORE_FILE
  );
  registerCleanupFunction(async () => {
    await cleanup();
  });
});

// Ensure the test only function deletes the store.
add_task(async function delete_store() {
  let storePath = await createCorruptedStore();
  await store.testDelete();
  Assert.ok(!(await IOUtils.exists(storePath)), "Store doesn't exist.");
});

/**
 * These tests check common no fail scenarios.
 */

add_task(async function init_insert_uninit() {
  await store.init();
  let result = await store.getCategories("foo");
  Assert.deepEqual(result, [], "foo should not have a result.");
  Assert.equal(
    await store.getVersion(),
    0,
    "Version number should not be set."
  );
  Assert.equal(store.empty, true, "Store should be empty.");

  info("Try inserting after init.");
  await store.insertFileContents(fileContents, 1);

  result = await store.getCategories("foo");
  Assert.deepEqual(result, [0, 1], "foo should have a matching result.");
  Assert.equal(await store.getVersion(), 1, "Version number should be set.");
  Assert.equal(store.empty, false, "Store should not be empty.");

  info("Un-init store.");
  await store.uninit();

  result = await store.getCategories("foo");
  Assert.deepEqual(result, [], "foo should be removed from store.");
  Assert.equal(store.empty, true, "Store should be empty.");
  Assert.equal(await store.getVersion(), 0, "Version should be reset.");

  await cleanup();
});

add_task(async function insert_and_re_init() {
  await store.init();
  await store.insertFileContents(fileContents, 20240202);

  let result = await store.getCategories("foo");
  Assert.deepEqual(result, [0, 1], "foo should have a matching result.");
  Assert.equal(
    await store.getVersion(),
    20240202,
    "Version number should be set."
  );
  Assert.equal(store.empty, false, "Is store empty.");

  info("Simulate a restart.");
  await store.uninit();
  await store.init();

  result = await store.getCategories("foo");
  Assert.deepEqual(
    result,
    [0, 1],
    "After restart, foo should still be in the store."
  );
  Assert.equal(
    await store.getVersion(),
    20240202,
    "Version number should still be in the store."
  );
  Assert.equal(store.empty, false, "Is store empty.");

  await cleanup();
});

// Simulate consecutive updates.
add_task(async function insert_multiple_times() {
  await store.init();
  let result = await store.getCategories("foo");
  Assert.deepEqual(result, [], "foo should not have a result.");
  Assert.equal(
    await store.getVersion(),
    0,
    "Version number should not be set."
  );
  Assert.equal(store.empty, true, "Is store empty.");

  for (let i = 0; i < 3; ++i) {
    info("Try inserting after init.");
    await store.insertFileContents(fileContents, 1);

    result = await store.getCategories("foo");
    Assert.deepEqual(result, [0, 1], "foo should have a matching result.");
    Assert.equal(store.empty, false, "Is store empty.");
    Assert.equal(await store.getVersion(), 1, "Version number is set.");

    await store.dropData();
    result = await store.getCategories("foo");
    Assert.deepEqual(
      result,
      [],
      "After dropping data, foo should no longer have a matching result."
    );
    Assert.equal(await store.getVersion(), 0, "Version should be reset.");
    Assert.equal(store.empty, true, "Is store empty.");
  }

  await cleanup();
});

/**
 * The following tests check failures on store initialization.
 */

add_task(async function init_with_corrupted_store() {
  await createCorruptedStore();

  info("Initialize the store.");
  await store.init();

  info("Try inserting after the corrupted store was replaced.");
  await store.insertFileContents(fileContents, 1);

  let result = await store.getCategories("foo");
  Assert.deepEqual(result, [0, 1], "foo should have a matching result.");
  Assert.equal(await store.getVersion(), 1, "Version number is set.");
  Assert.equal(store.empty, false, "Is store empty.");

  await cleanup();
});

add_task(async function init_with_unfixable_store() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(Sqlite, "openConnection").throws();

  info("Initialize the store.");
  await store.init();

  info("Try inserting content even if the connection is impossible to fix.");
  await store.dropData();
  await store.insertFileContents(fileContents, 20240202);

  let result = await store.getCategories("foo");
  Assert.deepEqual(result, [], "foo should not have a result.");
  Assert.equal(await store.getVersion(), 0, "Version should be reset.");
  Assert.equal(store.empty, true, "Store should be empty.");

  sandbox.restore();
  await cleanup();
});

add_task(async function init_read_only_store() {
  await createReadOnlyStore();
  await store.init();

  info("Insert contents into the store.");
  await store.insertFileContents(fileContents, 20240202);
  let result = await store.getCategories("foo");
  Assert.deepEqual(result, [], "foo should not have a result.");
  Assert.equal(
    await store.getVersion(),
    0,
    "Version number should not be set."
  );
  Assert.equal(store.empty, true, "Store should be empty.");

  await cleanup();
});

add_task(async function init_close_to_shutdown() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(Sqlite.shutdown, "addBlocker").throws(new Error());
  await store.init();

  let result = await store.getCategories("foo");
  Assert.deepEqual(result, [], "foo should not have a result.");
  Assert.equal(
    await store.getVersion(),
    0,
    "Version number should not be set."
  );
  Assert.equal(store.empty, true, "Store should be empty.");

  sandbox.restore();
  await cleanup();
});

/**
 * The following tests check error handling when inserting data into the store.
 */

add_task(async function insert_broken_file() {
  await store.init();

  Assert.equal(
    await store.getVersion(),
    0,
    "Version number should not be set."
  );

  info("Try inserting one valid file and an invalid file.");
  let contents = [...fileContents, new ArrayBuffer(0).buffer];
  await store.insertFileContents(contents, 20240202);

  let result = await store.getCategories("foo");
  Assert.deepEqual(result, [], "foo should not have a result.");
  Assert.equal(await store.getVersion(), 0, "Version should remain unset.");
  Assert.equal(store.empty, true, "Store should remain empty.");

  await cleanup();
});

add_task(async function insert_into_read_only_store() {
  await createReadOnlyStore();
  await store.init();

  await store.dropData();
  await store.insertFileContents(fileContents, 20240202);
  let result = await store.getCategories("foo");
  Assert.deepEqual(result, [], "foo should not have a result.");
  Assert.equal(await store.getVersion(), 0, "Version should remain unset.");
  Assert.equal(store.empty, true, "Store should remain empty.");

  await cleanup();
});

// If the store becomes read only with content already inside of it,
// the next time we try opening it, we'll encounter an error trying to write to
// it. Since we are no longer able to manipulate it, the results should always
// be empty.
add_task(async function restart_with_read_only_store() {
  await store.init();
  await store.insertFileContents(fileContents, 20240202);

  info("Check store has content.");
  let result = await store.getCategories("foo");
  Assert.deepEqual(result, [0, 1], "foo should have a matching result.");
  Assert.equal(
    await store.getVersion(),
    20240202,
    "Version number should be set."
  );
  Assert.equal(store.empty, false, "Store should not be empty.");

  await changeStoreToReadOnly();
  await store.uninit();
  await store.init();

  result = await store.getCategories("foo");
  Assert.deepEqual(
    result,
    [],
    "foo should no longer have a matching value from the store."
  );
  Assert.equal(await store.getVersion(), 0, "Version number should be unset.");
  Assert.equal(store.empty, true, "Store should be empty.");

  await cleanup();
});
