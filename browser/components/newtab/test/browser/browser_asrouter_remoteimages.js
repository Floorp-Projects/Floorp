/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);
const { Downloader } = ChromeUtils.import(
  "resource://services-settings/Attachments.jsm"
);
const {
  RemoteImages,
  REMOTE_IMAGES_PATH,
  REMOTE_IMAGES_DB_PATH,
} = ChromeUtils.import("resource://activity-stream/lib/RemoteImages.jsm");
const { RemoteImagesTestUtils } = ChromeUtils.import(
  "resource://testing-common/RemoteImagesTestUtils.jsm"
);

function dbWriteFinished(db) {
  // RemoteImages calls JSONFile.saveSoon(), so make sure that the DeferredTask
  // has finished saving the file.
  return BrowserTestUtils.waitForCondition(() => !db._saver.isArmed);
}

add_setup(RemoteImagesTestUtils.wipeCache);

add_task(async function test_remoteImages_load() {
  const imageInfo = RemoteImagesTestUtils.images.AboutRobots;

  async function runLoadTest(recordId, imageId) {
    const urls = await RemoteImages.load(imageId);
    Assert.equal(urls.size, 1, "RemogeImages.load() returns one result");

    const url = urls.get(imageId);
    Assert.ok(url.startsWith("blob:"), "RemoteImages.load() returns blob URL");
    Assert.ok(
      await IOUtils.exists(PathUtils.join(REMOTE_IMAGES_PATH, recordId)),
      "RemoteImages caches the image"
    );

    const data = new Uint8Array(
      await fetch(url, { credentials: "omit" }).then(rsp => rsp.arrayBuffer())
    );
    const expected = new Uint8Array(
      await fetch(imageInfo.url, { credentials: "omit" }).then(rsp =>
        rsp.arrayBuffer()
      )
    );

    Assert.equal(
      data.length,
      expected.length,
      "Data read from blob: URL has correct length"
    );
    Assert.deepEqual(
      data,
      expected,
      "Data read from blob: URL matches expected"
    );

    RemoteImages.unload(urls);
  }

  const stop = await RemoteImagesTestUtils.serveRemoteImages(imageInfo);

  try {
    info("Loading a RemoteImage with a record ID");
    await runLoadTest(imageInfo.recordId, imageInfo.recordId);

    info(
      "Loading a RemoteImage with a legacy image ID (record ID + extension)"
    );
    await runLoadTest(imageInfo.recordId, `${imageInfo.recordId}.png`);
  } finally {
    await stop();
    await RemoteImagesTestUtils.wipeCache();
  }
});

add_task(async function test_remoteImages_load_fallback() {
  const imageInfo = RemoteImagesTestUtils.images.AboutRobots;
  const origDbContent = {
    version: 1,
    images: {
      ...RemoteImagesTestUtils.dbEntryFor(imageInfo),
    },
  };

  await IOUtils.writeJSON(REMOTE_IMAGES_DB_PATH, origDbContent);

  const stop = await RemoteImagesTestUtils.serveRemoteImages(imageInfo);

  try {
    const urls = await RemoteImages.load(imageInfo.recordId);
    RemoteImages.unload(urls);

    await RemoteImages.withDb(async db => {
      Assert.ok(
        imageInfo.recordId in db.data.images,
        "RemoteImages DB entry present"
      );

      Assert.notDeepEqual(
        db.data,
        origDbContent,
        "RemoteImages DB changed after load()"
      );

      await dbWriteFinished(db);

      const onDisk = await IOUtils.readJSON(REMOTE_IMAGES_DB_PATH);
      Assert.deepEqual(
        db.data,
        onDisk,
        "RemoteImages DB was saved to disk after load() falled back to download"
      );
    });
  } finally {
    await stop();
    await RemoteImagesTestUtils.wipeCache();
  }
});

add_task(async function test_remoteImages_expire() {
  const THIRTY_ONE_DAYS = 31 * 24 * 60 * 60;
  const IMAGE_INFO = RemoteImagesTestUtils.images.AboutRobots;

  try {
    await RemoteImagesTestUtils.writeImage(IMAGE_INFO);
    await IOUtils.writeJSON(REMOTE_IMAGES_DB_PATH, {
      version: 1,
      images: {
        ...RemoteImagesTestUtils.dbEntryFor(
          IMAGE_INFO,
          Date.now() - THIRTY_ONE_DAYS
        ),
      },
    });

    await RemoteImagesTestUtils.triggerCleanup();
    await RemoteImages.withDb(async db => {
      Assert.deepEqual(
        db.data.images,
        [],
        "RemoteImages should have no images"
      );
      Assert.ok(
        !(await IOUtils.exists(
          PathUtils.join(REMOTE_IMAGES_PATH, IMAGE_INFO.recordId)
        )),
        "Image should have been deleted during cleanup"
      );
    });
  } finally {
    await RemoteImagesTestUtils.wipeCache();
  }
});

add_task(async function test_remoteImages_migrate() {
  try {
    await IOUtils.remove(REMOTE_IMAGES_DB_PATH);
    await RemoteImagesTestUtils.writeImage(
      RemoteImagesTestUtils.images.AboutRobots,
      RemoteImagesTestUtils.images.AboutRobots.filename
    );

    await RemoteImages.withDb(async db => {
      const children = await IOUtils.getChildren(REMOTE_IMAGES_PATH);
      Assert.deepEqual(
        children,
        [],
        "RemoteImages migration should delete all images."
      );

      await dbWriteFinished(db);
      const fromDisk = await IOUtils.readJSON(REMOTE_IMAGES_DB_PATH);

      Assert.deepEqual(
        fromDisk,
        db.data,
        "RemoteImages DB data should match on-disk contents"
      );

      Assert.equal(db.data.version, 1, "RemoteImages DB version should be 1");
      Assert.deepEqual(
        db.data.images,
        {},
        "RemoteImages DB should have no entries"
      );
    });
  } finally {
    await RemoteImagesTestUtils.wipeCache();
  }
});

add_task(async function test_remoteImages_load_dedup() {
  const imageInfo = RemoteImagesTestUtils.images.AboutRobots;
  const sandbox = sinon.createSandbox();

  const downloadSpy = sandbox.spy(Downloader.prototype, "downloadAsBytes");

  const images = ["about-robots", "about-robots", "about-robots"];

  const stop = await RemoteImagesTestUtils.serveRemoteImages(imageInfo);

  try {
    const results = await RemoteImages.load(...images);
    RemoteImages.unload(results);

    Assert.equal(
      results.size,
      1,
      "RemoteImages.load doesn't return duplicates"
    );

    Assert.ok(
      downloadSpy.calledOnce,
      "RemoteImages.load doesn't download duplicates"
    );
  } finally {
    await stop();
    await RemoteImagesTestUtils.triggerCleanup();
    sandbox.restore();
  }
});
