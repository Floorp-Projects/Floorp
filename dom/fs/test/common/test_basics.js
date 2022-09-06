/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

exported_symbols.testGetDirectoryDoesNotThrow = async function() {
  await navigator.storage.getDirectory();

  Assert.ok(true, "Should not have thrown");
};

exported_symbols.testGetDirectoryKindIsDirectory = async function() {
  const root = await navigator.storage.getDirectory();

  Assert.equal(root.kind, "directory");
};

exported_symbols.testDirectoryHandleStringConversion = async function() {
  const root = await navigator.storage.getDirectory();

  Assert.equal(
    "" + root,
    "[object FileSystemDirectoryHandle]",
    "Is directoryHandle convertible to string?"
  );
};

exported_symbols.testNewDirectoryHandleFromPrototype = async function() {
  const root = await navigator.storage.getDirectory();

  try {
    Object.create(root.prototype);
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(true, "Should have thrown");
    Assert.ok(ex instanceof TypeError, "Threw the right error type");
  }
};

exported_symbols.testDirectoryHandleSupportsKeysIterator = async function() {
  const root = await navigator.storage.getDirectory();

  const it = await root.keys();
  Assert.ok(!!it, "Does root support keys iterator?");
};

exported_symbols.testKeysIteratorNextIsCallable = async function() {
  const root = await navigator.storage.getDirectory();

  const it = await root.keys();
  Assert.ok(!!it, "Does root support keys iterator?");

  const item = await it.next();
  Assert.ok(!!item, "Should return an item");
};

exported_symbols.testDirectoryHandleSupportsValuesIterator = async function() {
  const root = await navigator.storage.getDirectory();

  const it = await root.values();
  Assert.ok(!!it, "Does root support values iterator?");
};

exported_symbols.testValuesIteratorNextIsCallable = async function() {
  const root = await navigator.storage.getDirectory();

  const it = await root.values();
  Assert.ok(!!it, "Does root support values iterator?");

  const item = await it.next();
  Assert.ok(!!item, "Should return an item");
};

exported_symbols.testDirectoryHandleSupportsEntriesIterator = async function() {
  const root = await navigator.storage.getDirectory();

  const it = await root.entries();
  Assert.ok(!!it, "Does root support entries iterator?");
};

exported_symbols.testEntriesIteratorNextIsCallable = async function() {
  const root = await navigator.storage.getDirectory();

  const it = await root.entries();
  Assert.ok(!!it, "Does root support entries iterator?");

  const item = await it.next();
  Assert.ok(!!item, "Should return an item");
};

exported_symbols.testGetFileHandleIsCallable = async function() {
  const root = await navigator.storage.getDirectory();
  const allowCreate = { create: true };

  const item = await root.getFileHandle("fileName", allowCreate);
  Assert.ok(!!item, "Should return an item");
};

exported_symbols.testGetDirectoryHandleIsCallable = async function() {
  const root = await navigator.storage.getDirectory();
  const allowCreate = { create: true };

  const item = await root.getDirectoryHandle("dirName", allowCreate);
  Assert.ok(!!item, "Should return an item");
};

exported_symbols.testRemoveEntryIsCallable = async function() {
  const root = await navigator.storage.getDirectory();
  const removeOptions = { recursive: true };

  await root.removeEntry("fileName", removeOptions);
  await root.removeEntry("dirName", removeOptions);
  try {
    await root.removeEntry("doesNotExist", removeOptions);
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(true, "Should have thrown");
    Assert.equal(
      ex.message,
      "Unknown failure",
      "Threw the right error message"
    );
  }
};

exported_symbols.testResolveIsCallable = async function() {
  const root = await navigator.storage.getDirectory();

  try {
    await root.resolve(root);

    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(true, "Should have thrown");
    Assert.equal(
      ex.result,
      Cr.NS_ERROR_NOT_IMPLEMENTED,
      "Threw the right result code"
    );
  }
};

for (const [key, value] of Object.entries(exported_symbols)) {
  Object.defineProperty(value, "name", {
    value: key,
    writable: false,
  });
}
