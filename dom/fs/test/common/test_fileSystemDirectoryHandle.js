/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

exported_symbols.smokeTest = async function smokeTest() {
  const storage = navigator.storage;
  const subdirectoryNames = new Set(["Documents", "Downloads", "Music"]);
  const allowCreate = { create: true };

  {
    let root = await storage.getDirectory();
    Assert.ok(root, "Can we access the root directory?");

    let it = await root.values();
    Assert.ok(!!it, "Does root have values iterator?");

    let elem = await it.next();
    Assert.ok(elem.done, "Is root directory empty?");

    for (let dirName of subdirectoryNames) {
      await root.getDirectoryHandle(dirName, allowCreate);
      Assert.ok(true, "Was it possible to add subdirectory " + dirName + "?");
    }
  }

  {
    let root = await storage.getDirectory();
    Assert.ok(root, "Can we refresh the root directory?");

    let it = await root.values();
    Assert.ok(!!it, "Does root have values iterator?");

    let hasElements = false;
    let hangGuard = 0;
    for await (let [key, elem] of root.entries()) {
      Assert.ok(elem, "Is element not non-empty?");
      Assert.equal("directory", elem.kind, "Is found item a directory?");
      Assert.ok(
        elem.name.length >= 1 && elem.name.match("^[A-Za-z]{1,64}"),
        "Are names of the elements strings?"
      );
      Assert.equal(key, elem.name);
      Assert.ok(subdirectoryNames.has(elem.name), "Is name among known names?");
      hasElements = true;
      ++hangGuard;
      if (hangGuard == 10) {
        break; // Exit if there is a hang
      }
    }

    Assert.ok(hasElements, "Is values container now non-empty?");
    Assert.equal(3, hangGuard, "Do we only have three elements?");

    {
      it = await root.values();
      Assert.ok(!!it, "Does root have values iterator?");
      let elem = await it.next();

      await elem.value.getDirectoryHandle("Trash", allowCreate);
      let subit = elem.value.values();
      Assert.ok(!!elem, "Is element not non-empty?");
      let subdirResult = await subit.next();
      let subdir = subdirResult.value;
      Assert.ok(!!subdir, "Is element not non-empty?");
      Assert.equal("directory", subdir.kind, "Is found item a directory?");
      Assert.equal("Trash", subdir.name, "Is found item a directory?");
    }

    const wipeEverything = { recursive: true };
    for (let dirName of subdirectoryNames) {
      await root.removeEntry(dirName, wipeEverything);
      Assert.ok(
        true,
        "Was it possible to remove subdirectory " + dirName + "?"
      );
    }
  }

  {
    let root = await storage.getDirectory();
    Assert.ok(root, "Can we refresh the root directory?");

    let it = root.values();
    Assert.ok(!!it, "Does root have values iterator?");

    let elem = await it.next();
    Assert.ok(elem.done, "Is root directory empty?");
  }
};

for (const [key, value] of Object.entries(exported_symbols)) {
  Object.defineProperty(value, "name", {
    value: key,
    writable: false,
  });
}
