/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const allowCreate = { create: true };

exported_symbols.test0 = async function () {
  let root = await navigator.storage.getDirectory();
  Assert.ok(!!root, "Can we access the root directory?");

  const testFile = await root.getFileHandle("test.txt", allowCreate);
  Assert.ok(!!testFile, "Can't access existing file");
  let writable = await testFile.createWritable();
  Assert.ok(!!writable, "Can't create WritableFileStream to existing file");

  // Write a sentence to the end of the file.
  const encoder = new TextEncoder();
  const writeBuffer = encoder.encode("Thank you for reading this.");
  try {
    dump("Trying to write...\n");
    await writable.write(writeBuffer);
    dump("closing...\n");
    await writable.close();
  } catch (e) {
    Assert.ok(false, "Couldn't write to WritableFileStream: " + e);
  }

  // Read it back
  // Get size of the file.
  let file = await testFile.getFile();
  Assert.ok(
    !!file,
    "Can't create File to file written with WritableFileStream"
  );
  let fileSize = file.size;
  Assert.ok(fileSize == writeBuffer.byteLength);
};

exported_symbols.quotaTest = async function () {
  const shrinkedStorageSizeKB = 5 * 1024;
  const defaultDatabaseSize = 294912;

  // Shrink storage size to 5MB.
  await Utils.shrinkStorageSize(shrinkedStorageSizeKB);

  let root = await navigator.storage.getDirectory();
  Assert.ok(root, "Can we access the root directory?");

  // Fill entire storage.
  const fileHandle = await root.getFileHandle("test.txt", allowCreate);
  Assert.ok(!!fileHandle, "Can we get file handle?");

  const writable = await fileHandle.createWritable();
  Assert.ok(!!writable, "Can we create writable file stream?");

  const buffer = new ArrayBuffer(
    shrinkedStorageSizeKB * 1024 - defaultDatabaseSize
  );
  Assert.ok(!!buffer, "Can we create array buffer?");

  const result = await writable.write(buffer);
  Assert.equal(result, undefined, "Can we write entire buffer?");

  // Try to write one more byte.
  const fileHandle2 = await root.getFileHandle("test2.txt", allowCreate);
  Assert.ok(!!fileHandle2, "Can we get file handle?");

  const writable2 = await fileHandle2.createWritable();
  Assert.ok(!!writable2, "Can we create writable file stream?");

  const buffer2 = new ArrayBuffer(1);
  Assert.ok(!!buffer2, "Can we create array buffer?");

  try {
    await writable2.write(buffer2);
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(true, "Did throw");
    Assert.ok(DOMException.isInstance(ex), "Threw DOMException");
    Assert.equal(ex.name, "QuotaExceededError", "Threw right DOMException");
  }

  await writable.close();
  // writable2 is already closed because of the failed write above

  await Utils.restoreStorageSize();
};

for (const [key, value] of Object.entries(exported_symbols)) {
  Object.defineProperty(value, "name", {
    value: key,
    writable: false,
  });
}
