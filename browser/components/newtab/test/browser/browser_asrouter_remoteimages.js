/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { BrowserUtils } = ChromeUtils.import(
  "resource://gre/modules/BrowserUtils.jsm"
);
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);
const { Downloader } = ChromeUtils.import(
  "resource://services-settings/Attachments.jsm"
);
const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { PanelTestProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/PanelTestProvider.jsm"
);
const {
  RemoteImages,
  REMOTE_IMAGES_PATH,
  REMOTE_IMAGES_DB_PATH,
} = ChromeUtils.import("resource://activity-stream/lib/RemoteImages.jsm");
const { RemoteImagesTestUtils, RemoteSettingsServer } = ChromeUtils.import(
  "resource://testing-common/RemoteImagesTestUtils.jsm"
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { RemoteSettingsExperimentLoader } = ChromeUtils.import(
  "resource://nimbus/lib/RemoteSettingsExperimentLoader.jsm"
);

const PREFETCH_FINISHED_TOPIC = "remote-images:prefetch-finished";

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
    await RemoteImagesTestUtils.wipeCache();
    sandbox.restore();
  }
});

add_task(async function test_remoteImages_sync() {
  const { images } = RemoteImagesTestUtils;
  const sandbox = sinon.createSandbox();

  const server = new RemoteSettingsServer();
  server.start();
  const client = RemoteSettings("ms-images");
  client.verifySignatures = false;

  try {
    server.addRemoteImage(images.AboutRobots);

    const downloadSpy = sandbox.spy(Downloader.prototype, "downloadAsBytes");
    RemoteImages.unload(await RemoteImages.load(images.AboutRobots.recordId));

    Assert.ok(downloadSpy.calledOnce);
    downloadSpy.resetHistory();

    const lastLoad = await RemoteImages.withDb(
      db => db.data.images[images.AboutRobots.recordId].lastLoaded
    );

    {
      const { filename, mimetype, hash, url, size } = images.Mountain;
      const location = `main/ms-images/${images.AboutRobots.recordId}`;
      Object.assign(server.attachments.get(location), { mimetype, size, url });

      // This isn't strictly necessary because we won't be re-fetching the
      // record from the server -- instead we will be relying on the cached
      // database for the ms-images RemoteSettingsClient.
      server.buckets
        .update("main")
        .update("ms-images")
        .update(images.AboutRobots.recordId)
        .set({
          attachment: {
            filename,
            location,
            hash,
            mimetype,
            size,
          },
        });
    }

    // Fake a Remote Settings sync. We update the client's database and then
    // tell the client we've finished syncing. This will trigger RemoteImages
    // to check the DB for updated records.
    await client.db.importChanges(
      {},
      Date.now(),
      [
        server.buckets
          .get("main")
          .get("ms-images")
          .get(images.AboutRobots.recordId).data,
      ],
      { clear: true }
    );
    await client.emit("sync", { data: {} });

    Assert.ok(downloadSpy.calledOnce);

    await RemoteImages.withDb(async db => {
      Assert.equal(
        db.data.images[images.AboutRobots.recordId].hash,
        images.Mountain.hash
      );

      Assert.equal(
        db.data.images[images.AboutRobots.recordId].lastLoaded,
        lastLoad,
        "Fetching an image should not change lastLoaded"
      );
    });
  } finally {
    await server.stop();
    await RemoteImagesTestUtils.wipeCache();
    sandbox.restore();
    client.verifySignatures = true;
  }
});

add_task(async function test_remoteImages_prefetch() {
  const { AboutRobots } = RemoteImagesTestUtils.images;
  const sandbox = sinon.createSandbox();

  sandbox.stub(RemoteSettingsExperimentLoader, "setTimer");
  sandbox.stub(RemoteSettingsExperimentLoader, "updateRecipes").resolves();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["app.shield.optoutstudies.enabled", true],
      ["datareporting.healthreport.uploadEnabled", true],
      [
        "browser.newtabpage.activity-stream.asrouter.providers.messaging-experiments",
        `{"id":"messaging-experiments","enabled":true,"type":"remote-experiments","updateCycleInMs":0}`,
      ],
    ],
  });

  const stop = await RemoteImagesTestUtils.serveRemoteImages(AboutRobots);

  const message = await PanelTestProvider.getMessages().then(msgs =>
    msgs.find(m => m.id === "SPOTLIGHT_MESSAGE_93")
  );
  message.content.logo = { imageId: AboutRobots.recordId };
  const recipe = ExperimentFakes.recipe("spotlight-test", {
    branches: [
      {
        slug: "snail",
        ratio: 1,
        features: [
          {
            featureId: "spotlight",
            value: message,
          },
        ],
      },
    ],
    bucketConfig: {
      start: 0,
      count: 100,
      total: 100,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
  });

  Assert.ok(ASRouter.initialized, "ASRouter should be initialized");

  const prefetchFinished = BrowserUtils.promiseObserved(
    PREFETCH_FINISHED_TOPIC
  );

  const {
    enrollmentPromise,
    doExperimentCleanup,
  } = ExperimentFakes.enrollmentHelper(recipe);

  await Promise.all([enrollmentPromise, prefetchFinished]);

  try {
    await RemoteImages.withDb(async db => {
      const entry = db.data.images[AboutRobots.recordId];

      Assert.equal(
        entry.recordId,
        AboutRobots.recordId,
        "Prefetched image DB entry should exist"
      );
      Assert.equal(
        entry.mimetype,
        AboutRobots.mimetype,
        "Prefetched image should have correct mimetype"
      );
      Assert.equal(
        entry.hash,
        AboutRobots.hash,
        "Prefetched image should have correct hash"
      );
    });

    const path = PathUtils.join(REMOTE_IMAGES_PATH, AboutRobots.recordId);
    await BrowserTestUtils.waitForCondition(
      () => IOUtils.exists(path),
      "Prefetched image should be written to disk"
    );

    Assert.equal(
      await IOUtils.computeHexDigest(path, "sha256"),
      AboutRobots.hash,
      "AboutRobots image should have correct hash"
    );
  } finally {
    await doExperimentCleanup();
    await stop();
    await SpecialPowers.popPrefEnv();
    await ASRouter._updateMessageProviders();
    await RemoteImagesTestUtils.wipeCache();
    sandbox.restore();
  }
});
