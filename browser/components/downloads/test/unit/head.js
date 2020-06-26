const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "Downloads",
  "resource://gre/modules/Downloads.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadsCommon",
  "resource:///modules/DownloadsCommon.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "FileTestUtils",
  "resource://testing-common/FileTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TestUtils",
  "resource://testing-common/TestUtils.jsm"
);

async function createDownloadedFile(pathname, contents) {
  info("createDownloadedFile: " + pathname);
  let encoder = new TextEncoder();
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
    await OS.File.writeAtomic(pathname, encoder.encode(contents));
    ok(file.exists(), `Created ${pathname}`);
  }
  // No post-test cleanup necessary; tmp downloads directory is already removed after each test
  return file;
}

let gDownloadDir;

async function setDownloadDir() {
  let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tmpDir.append("testsavedir");
  if (!tmpDir.exists()) {
    tmpDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    registerCleanupFunction(function() {
      try {
        tmpDir.remove(true);
      } catch (e) {
        // On Windows debug build this may fail.
      }
    });
  }
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setCharPref("browser.download.dir", tmpDir.path);
  return tmpDir.path;
}

/**
 * All the tests are implemented with add_task, this starts them automatically.
 */
function run_test() {
  do_get_profile();
  run_next_test();
}

add_task(async function test_common_initialize() {
  gDownloadDir = await setDownloadDir();
  Services.prefs.setCharPref("browser.download.loglevel", "Debug");
});
