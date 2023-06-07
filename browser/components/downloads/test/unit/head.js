ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  DownloadsCommon: "resource:///modules/DownloadsCommon.sys.mjs",
  FileTestUtils: "resource://testing-common/FileTestUtils.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

async function createDownloadedFile(pathname, contents) {
  info("createDownloadedFile: " + pathname);
  let file = new FileUtils.File(pathname);
  if (file.exists()) {
    info(`File at ${pathname} already exists`);
    if (!contents) {
      ok(
        false,
        `A file already exists at ${pathname}, but createDownloadedFile was asked to create a non-existant file`
      );
    }
  }
  if (contents) {
    await IOUtils.writeUTF8(pathname, contents);
    ok(file.exists(), `Created ${pathname}`);
  }
  // No post-test cleanup necessary; tmp downloads directory is already removed after each test
  return file;
}

let gDownloadDir;

async function setDownloadDir() {
  let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile).path;
  tmpDir = PathUtils.join(
    tmpDir,
    "testsavedir" + Math.floor(Math.random() * 2 ** 32)
  );
  // Create this dir if it doesn't exist (ignores existing dirs)
  await IOUtils.makeDirectory(tmpDir);
  registerCleanupFunction(async function () {
    try {
      await IOUtils.remove(tmpDir, { recursive: true });
    } catch (e) {
      console.error(e);
    }
  });
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setCharPref("browser.download.dir", tmpDir);
  return tmpDir;
}

/**
 * All the tests are implemented with add_task, this starts them automatically.
 */
function run_test() {
  do_get_profile();
  run_next_test();
}

add_setup(async function test_common_initialize() {
  gDownloadDir = await setDownloadDir();
  Services.prefs.setCharPref("browser.download.loglevel", "Debug");
});
