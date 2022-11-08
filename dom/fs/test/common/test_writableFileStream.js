/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const allowCreate = { create: true };

exported_symbols.test0 = async function() {
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

for (const [key, value] of Object.entries(exported_symbols)) {
  Object.defineProperty(value, "name", {
    value: key,
    writable: false,
  });
}
