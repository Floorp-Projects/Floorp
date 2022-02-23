/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ObjectUtils } = ChromeUtils.import(
  "resource://gre/modules/ObjectUtils.jsm"
);
const { RemoteImages } = ChromeUtils.import(
  "resource://activity-stream/lib/RemoteImages.jsm"
);
const { RemoteImagesTestUtils } = ChromeUtils.import(
  "resource://testing-common/RemoteImagesTestUtils.jsm"
);

add_task(async function test_remoteImages_load() {
  const imageInfo = RemoteImagesTestUtils.images.AboutRobots;
  const cleanup = await RemoteImagesTestUtils.serveRemoteImages(imageInfo);

  registerCleanupFunction(cleanup);

  const url = await RemoteImages.load(imageInfo.imageId);
  ok(url.startsWith("blob:"), "RemoteImages.load() returns blob URL");
  ok(
    await IOUtils.exists(
      PathUtils.join(RemoteImages.imagesDir, imageInfo.imageId)
    ),
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

  is(
    data.length,
    expected.length,
    "Data read from blob: URL has correct length"
  );
  ok(
    ObjectUtils.deepEqual(data, expected),
    "Data read from blob: URL matches expected"
  );

  RemoteImages.unload(url);
});

add_task(async function test_remoteImages_expire() {
  const THIRTY_ONE_DAYS = 31 * 24 * 60 * 60;

  const imageInfo = RemoteImagesTestUtils.images.AboutRobots;

  const cleanup = await RemoteImagesTestUtils.serveRemoteImages(imageInfo);
  registerCleanupFunction(cleanup);

  const path = PathUtils.join(RemoteImages.imagesDir, imageInfo.imageId);

  info("Doing initial load() of image");
  {
    const url = await RemoteImages.load(imageInfo.imageId);
    RemoteImages.unload(url);
  }

  ok(
    await IOUtils.exists(path),
    "Remote image exists after load() and unload()"
  );

  await RemoteImagesTestUtils.triggerCleanup();
  ok(
    await IOUtils.exists(path),
    "Remote image cache still exists after cleanup because it has not expired"
  );

  info("Setting last modified time beyond expiration date and reloading");
  {
    await IOUtils.setModificationTime(path, Date.now() - THIRTY_ONE_DAYS);

    const url = await RemoteImages.load(imageInfo.imageId);
    RemoteImages.unload(url);
  }

  await RemoteImagesTestUtils.triggerCleanup();
  ok(
    await IOUtils.exists(path),
    "Remote image cache still exists after cleanup becuase load() refreshes the expiry"
  );

  info("Setting last mofieid time beyond expiration");
  await IOUtils.setModificationTime(path, Date.now() - THIRTY_ONE_DAYS);

  await RemoteImagesTestUtils.triggerCleanup();
  ok(
    !(await IOUtils.exists(path)),
    "Remote image cache no longer exists after clenaup beyond expiry"
  );
});
