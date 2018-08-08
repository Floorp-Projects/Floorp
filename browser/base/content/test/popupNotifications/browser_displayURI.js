/*
 * Make sure that the correct origin is shown for permission prompts.
 */

async function check(contentTask, options = {}) {
  await BrowserTestUtils.withNewTab("https://test1.example.com/", async function(browser) {
    let popupShownPromise = waitForNotificationPanel();
    await ContentTask.spawn(browser, null, contentTask);
    let panel = await popupShownPromise;
    let notification = panel.children[0];
    let body = document.getAnonymousElementByAttribute(notification,
                                                       "class",
                                                       "popup-notification-body");
    ok(body.innerHTML.includes("example.com"), "Check that at least the eTLD+1 is present in the markup");
  });

  let channel = NetUtil.newChannel({
    uri: getRootDirectory(gTestPath),
    loadUsingSystemPrincipal: true,
  });
  channel = channel.QueryInterface(Ci.nsIFileChannel);

  await BrowserTestUtils.withNewTab(channel.file.path, async function(browser) {
    let popupShownPromise = waitForNotificationPanel();
    await ContentTask.spawn(browser, null, contentTask);
    let panel = await popupShownPromise;
    let notification = panel.children[0];
    let body = document.getAnonymousElementByAttribute(notification,
                                                       "class",
                                                       "popup-notification-body");
    if (notification.id == "geolocation-notification") {
      ok(body.innerHTML.includes("local file"), `file:// URIs should be displayed as local file.`);
    } else {
      ok(body.innerHTML.includes("Unknown origin"), "file:// URIs should be displayed as unknown origin.");
    }
  });

  if (!options.skipOnExtension) {
    // Test the scenario also on the extension page if not explicitly unsupported
    // (e.g. an extension page can't be navigated on a blob URL).
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        name: "Test Extension Name",
      },
      background() {
        let {browser} = this;
        browser.test.sendMessage("extension-tab-url",
                                 browser.extension.getURL("extension-tab-page.html"));
      },
      files: {
        "extension-tab-page.html": `<!DOCTYPE html><html><body></body></html>`,
      },
    });

    await extension.startup();
    let extensionURI = await extension.awaitMessage("extension-tab-url");

    await BrowserTestUtils.withNewTab(extensionURI, async function(browser) {
      let popupShownPromise = waitForNotificationPanel();
      await ContentTask.spawn(browser, null, contentTask);
      let panel = await popupShownPromise;
      let notification = panel.children[0];
      let body = document.getAnonymousElementByAttribute(notification,
                                                         "class",
                                                         "popup-notification-body");
      ok(body.innerHTML.includes("Test Extension Name"),
         "Check the the extension name is present in the markup");
    });

    await extension.unload();
  }
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({set: [
    ["media.navigator.permission.fake", true],
    ["media.navigator.permission.force", true],
  ]});
});

add_task(async function test_displayURI_geo() {
  await check(async function() {
    content.navigator.geolocation.getCurrentPosition(() => {});
  });
});

add_task(async function test_displayURI_camera() {
  await check(async function() {
    content.navigator.mediaDevices.getUserMedia({video: true, fake: true});
  });
});

add_task(async function test_displayURI_geo_blob() {
  await check(async function() {
    let text = "<script>navigator.geolocation.getCurrentPosition(() => {})</script>";
    let blob = new Blob([text], {type: "text/html"});
    let url = content.URL.createObjectURL(blob);
    content.location.href = url;
  }, {skipOnExtension: true});
});

add_task(async function test_displayURI_camera_blob() {
  await check(async function() {
    let text = "<script>navigator.mediaDevices.getUserMedia({video: true, fake: true})</script>";
    let blob = new Blob([text], {type: "text/html"});
    let url = content.URL.createObjectURL(blob);
    content.location.href = url;
  }, {skipOnExtension: true});
});

