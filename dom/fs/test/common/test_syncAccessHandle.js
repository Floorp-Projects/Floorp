/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const allowCreate = { create: true };

exported_symbols.test0 = async function() {
  let root = await navigator.storage.getDirectory();
  Assert.ok(root, "Can we access the root directory?");

  try {
    await root.getFileHandle("test.txt");
    Assert.ok(false, "Opened file that shouldn't exist");
  } catch (e) {
    dump("caught exception when we tried to open a non-existant file\n");
  }
};

exported_symbols.test1 = async function() {
  let root = await navigator.storage.getDirectory();
  Assert.ok(root, "Can we access the root directory?");

  const testFile = await root.getFileHandle("test.txt", allowCreate);
  Assert.ok(!!testFile, "Can't create file");
  let handle = await testFile.createSyncAccessHandle();
  Assert.ok(!!handle, "Can't create SyncAccessHandle");
  await handle.close();
  handle = await testFile.createSyncAccessHandle();
  Assert.ok(!!handle, "Can't create second SyncAccessHandle to same file");
  await handle.close();
};

exported_symbols.test2 = async function() {
  let root = await navigator.storage.getDirectory();
  Assert.ok(root, "Can we access the root directory?");

  const testFile = await root.getFileHandle("test.txt");
  Assert.ok(!!testFile, "Can't open file");
  let handle = await testFile.createSyncAccessHandle();
  Assert.ok(!!handle, "Can't create SyncAccessHandle");
  await handle.close();

  await root.removeEntry("test.txt");
  try {
    handle = await testFile.createSyncAccessHandle();
    Assert.ok(!!handle, "Didn't remove file!");
    if (handle) {
      await handle.close();
    }
  } catch (e) {
    dump("Caught exception trying to create accesshandle to deleted file\n");
  }
};

exported_symbols.test3 = async function() {
  let root = await navigator.storage.getDirectory();
  Assert.ok(!!root, "Can we access the root directory?");

  let dir = await root.getDirectoryHandle("dir", allowCreate);
  Assert.ok(!!dir, "Can we create a directory?");

  // XXX not implemented yet
  //const path = await root.resolve(dir);
  //Assert.ok(path == ["dir"], "Wrong path: " + path);

  let dir2 = await dir.getDirectoryHandle("dir", allowCreate);
  Assert.ok(!!dir, "Can we create dir/dir?");

  // XXX not implemented yet
  //const path = await root.resolve(dir2);
  //Assert.ok(path == ["dir", "dir"], "Wrong path: " + path);

  let dir3 = await dir.getDirectoryHandle("bar", allowCreate);
  Assert.ok(!!dir3, "Can we create dir/bar?");

  // This should fail
  try {
    await root.getDirectoryHandle("bar");
    Assert.ok(!dir, "we shouldn't be able to get bar unless we create it");
  } catch (e) {
    dump("caught exception when we tried to get a non-existant dir\n");
  }

  const testFile = await dir2.getFileHandle("test.txt", allowCreate);
  Assert.ok(!!testFile, "Can't create file in dir2");
  let handle = await testFile.createSyncAccessHandle();
  Assert.ok(!!handle, "Can't create SyncAccessHandle in dir2");
  await handle.close();
};

exported_symbols.test4 = async function() {
  let root = await navigator.storage.getDirectory();
  Assert.ok(!!root, "Can we access the root directory?");

  const testFile = await root.getFileHandle("test.txt", allowCreate);
  Assert.ok(!!testFile, "Can't access existing file");
  let handle = await testFile.createSyncAccessHandle();
  Assert.ok(!!handle, "Can't create SyncAccessHandle to existing file");

  // Write a sentence to the end of the file.
  const encoder = new TextEncoder();
  const writeBuffer = encoder.encode("Thank you for reading this.");
  const writeSize = handle.write(writeBuffer);
  Assert.ok(!!writeSize);

  // Read it back
  // Get size of the file.
  let fileSize = await handle.getSize();
  Assert.ok(fileSize == writeBuffer.byteLength);
  // Read file content to a buffer.
  const readBuffer = new ArrayBuffer(fileSize);
  const readSize = handle.read(readBuffer, { at: 0 });
  Assert.ok(!!readSize);
  //Assert.ok(readBuffer == writeBuffer);

  await handle.truncate(5);
  fileSize = await handle.getSize();
  Assert.ok(fileSize == 5);

  await handle.flush();
  await handle.close();
};

exported_symbols.test5 = async function() {
  let root = await navigator.storage.getDirectory();
  Assert.ok(!!root, "Can we access the root directory?");

  const testFile = await root.getFileHandle("test.txt");
  Assert.ok(!!testFile, "Can't create file");
  let handle = await testFile.createSyncAccessHandle();
  Assert.ok(!!handle, "Can't create SyncAccessHandle");

  try {
    const testFile2 = await root.getFileHandle("test2.txt", allowCreate);
    let handle2 = await testFile2.createSyncAccessHandle();
    Assert.ok(!!handle2, "can't create SyncAccessHandle to second file!");
    if (handle2) {
      await handle2.close();
    }
  } catch (e) {
    Assert.ok(false, "Failed to create second file");
  }

  await handle.close();
};

exported_symbols.test6 = async function() {
  let root = await navigator.storage.getDirectory();
  Assert.ok(root, "Can we access the root directory?");

  const testFile = await root.getFileHandle("test.txt");
  Assert.ok(!!testFile, "Can't get file");
  let handle = await testFile.createSyncAccessHandle();
  Assert.ok(!!handle, "Can't create SyncAccessHandle");

  try {
    let handle2 = await testFile.createSyncAccessHandle();
    Assert.ok(!handle2, "Shouldn't create SyncAccessHandle!");
    if (handle2) {
      await handle2.close();
    }
  } catch (e) {
    // should always happen
    dump("caught exception when we tried to get 2 SyncAccessHandles\n");
  }

  // test that locks work across multiple connections for an origin
  try {
    let root2 = await navigator.storage.getDirectory();
    Assert.ok(root2, "Can we access the root2 directory?");

    const testFile2 = await root2.getFileHandle("test.txt");
    Assert.ok(!!testFile2, "Can't get file");
    let handle2 = await testFile2.createSyncAccessHandle();
    Assert.ok(!handle2, "Shouldn't create SyncAccessHandle (2)!");
    if (handle2) {
      await handle2.close();
    }
  } catch (e) {
    // should always happen
    dump("caught exception when we tried to get 2 SyncAccessHandles\n");
  }

  if (handle) {
    await handle.close();
  }
};

for (const [key, value] of Object.entries(exported_symbols)) {
  Object.defineProperty(value, "name", {
    value: key,
    writable: false,
  });
}
