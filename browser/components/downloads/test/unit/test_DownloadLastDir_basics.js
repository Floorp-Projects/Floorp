/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Basic test for setting and retrieving a download last dir.
// More complex tests can be found in browser/components/privatebrowsing/.

const SAVE_PER_SITE_PREF_BRANCH = "browser.download.lastDir";
const SAVE_PER_SITE_PREF = SAVE_PER_SITE_PREF_BRANCH + ".savePerSite";

let { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);
let { DownloadLastDir } = ChromeUtils.importESModule(
  "resource://gre/modules/DownloadLastDir.sys.mjs"
);

add_task(
  {
    pref_set: [[SAVE_PER_SITE_PREF, true]],
  },
  async function test() {
    let downloadLastDir = new DownloadLastDir(null);

    let unknownUri = Services.io.newURI("https://unknown.org/");
    Assert.deepEqual(
      await downloadLastDir.getFileAsync(unknownUri),
      null,
      "Untracked URI, no pref set"
    );

    let dir1 = FileUtils.getDir("TmpD", ["dir1"]);
    dir1.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    let uri1 = Services.io.newURI("https://test1.moz.org");
    downloadLastDir.setFile(uri1, dir1);
    let dir2 = FileUtils.getDir("TmpD", ["dir2"]);
    dir2.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    let uri2 = Services.io.newURI("https://test2.moz.org");
    downloadLastDir.setFile(uri2, dir2);
    let dir3 = FileUtils.getDir("TmpD", ["dir3"]);
    dir3.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    downloadLastDir.setFile(null, dir3);
    Assert.equal(
      (await downloadLastDir.getFileAsync(uri1)).path,
      dir1.path,
      "Check common URI"
    );
    Assert.equal(
      (await downloadLastDir.getFileAsync(uri2)).path,
      dir2.path,
      "Check common URI"
    );
    Assert.equal(downloadLastDir.file.path, dir3.path, "No URI");
    Assert.equal(
      (await downloadLastDir.getFileAsync(unknownUri)).path,
      dir3.path,
      "Untracked URI, pref set"
    );

    info("Check clearHistory removes all data");
    let subject = {};
    Services.obs.notifyObservers(subject, "browser:purge-session-history");
    await subject.promise;
    Assert.deepEqual(
      await downloadLastDir.getFileAsync(uri1),
      null,
      "Check common URI after clear history returns null"
    );
    Assert.deepEqual(
      await downloadLastDir.getFileAsync(uri2),
      null,
      "Check common URI after clear history returns null"
    );
    Assert.deepEqual(
      await downloadLastDir.getFileAsync(unknownUri),
      null,
      "Check untracked URI after clear history returns null"
    );

    // file: URIs should all point to the same folder.
    let fileUri1 = Services.io.newURI("file:///c:/test.txt");
    downloadLastDir.setFile(uri1, dir3);
    let dir4 = FileUtils.getDir("TmpD", ["dir4"]);
    dir4.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
    let fileUri2 = Services.io.newURI("file:///d:/test.png");
    downloadLastDir.setFile(uri1, dir4);
    Assert.equal(
      (await downloadLastDir.getFileAsync(fileUri1)).path,
      dir4.path,
      "Check file URI"
    );
    Assert.equal(
      (await downloadLastDir.getFileAsync(fileUri2)).path,
      dir4.path,
      "Check file URI"
    );
    let unknownFileUri = Services.io.newURI("file:///e:/test.mkv");
    Assert.equal(
      (await downloadLastDir.getFileAsync(unknownFileUri)).path,
      dir4.path,
      "Untracked File URI, pref set"
    );

    // data: URIs should point to a folder per mime-type.
    // Unspecified mime-type is handled as text/plain.
    let dataUri1 = Services.io.newURI("data:text/plain;charset=UTF-8,1234");
    downloadLastDir.setFile(dataUri1, dir1);
    let dataUri2 = Services.io.newURI("data:image/png;base64,1234");
    Assert.equal(
      (await downloadLastDir.getFileAsync(dataUri2)).path,
      dir1.path,
      "Check data URI"
    );
    let dataUri3 = Services.io.newURI("data:image/png,5678");
    downloadLastDir.setFile(dataUri3, dir2);
    Assert.equal(
      (await downloadLastDir.getFileAsync(dataUri2)).path,
      dir2.path,
      "Data URI was changed, same mime-type"
    );
    Assert.equal(
      (await downloadLastDir.getFileAsync(dataUri1)).path,
      dir1.path,
      "Data URI was not changed, different mime-type"
    );
    let dataUri4 = Services.io.newURI("data:,");
    Assert.equal(
      (await downloadLastDir.getFileAsync(dataUri4)).path,
      dir1.path,
      "Data URI defaults to text/plain"
    );
    downloadLastDir.setFile(null, dir4);
    let unknownDataUri = Services.io.newURI("data:application/zip,");
    Assert.deepEqual(
      (await downloadLastDir.getFileAsync(unknownDataUri)).path,
      dir4.path,
      "Untracked data URI"
    );
    Assert.equal(
      (await downloadLastDir.getFileAsync(dataUri4)).path,
      dir1.path,
      "Data URI didn't change"
    );

    info("blob: URIs should point to a folder based on their origin.");
    let blobUri1 = Services.io.newURI(
      "blob:https://chat.mozilla.org/35d6a992-6e18-4957-8216-070c53b9bc83"
    );
    let blobOriginUri1 = Services.io.newURI("https://chat.mozilla.org/");
    downloadLastDir.setFile(blobUri1, dir1);
    Assert.equal(
      (await downloadLastDir.getFileAsync(blobUri1)).path,
      (await downloadLastDir.getFileAsync(blobOriginUri1)).path,
      "Check blob URI"
    );
    // While we are no longer supposed to store pdf.js URLs like this, this
    // test remains to cover resource origins.
    info("Test blob: URIs to local resouce.");
    let blobUri2 = Services.io.newURI(
      "blob:resource://pdf.js/ed645567-3eea-4ff1-94fd-efb04812afe0"
    );
    let blobOriginUri2 = Services.io.newURI("resource://pdf.js/");
    downloadLastDir.setFile(blobUri2, dir2);
    Assert.equal(
      (await downloadLastDir.getFileAsync(blobUri2)).path,
      (await downloadLastDir.getFileAsync(blobOriginUri2)).path,
      "Check blob URI"
    );
    info("Test an empty blob:");
    let noOriginBlobUri = Services.io.newURI("blob:");
    downloadLastDir.setFile(blobUri2, dir3);
    Assert.equal(
      (await downloadLastDir.getFileAsync(noOriginBlobUri)).path,
      dir3.path,
      "Check blob URI"
    );
  }
);
