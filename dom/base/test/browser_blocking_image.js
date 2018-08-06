const TEST_URI = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com") + "file_blocking_image.html";

/**
 * Loading an image from https:// should work.
 */
add_task(async function load_image_from_https_test() {
  let tab = gBrowser.addTab(TEST_URI);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  gBrowser.selectedTab = tab;

  await ContentTask.spawn(tab.linkedBrowser, { }, async function() {
    function imgListener(img) {
      return new Promise((resolve, reject) => {
        img.addEventListener("load", () => resolve());
        img.addEventListener("error", () => reject());
      });
    }

    let img = content.document.createElement("img");
    img.src = "https://example.com/tests/image/test/mochitest/shaver.png";
    content.document.body.appendChild(img);

    try {
      await imgListener(img);
      Assert.ok(true);
    } catch (e) {
      Assert.ok(false);
    }

    Assert.equal(img.imageBlockingStatus, Ci.nsIContentPolicy.ACCEPT);
  });

  gBrowser.removeTab(tab);
});

/**
 * Loading an image from http:// should be rejected.
 */
add_task(async function load_image_from_http_test() {
  let tab = gBrowser.addTab(TEST_URI);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  gBrowser.selectedTab = tab;

  await ContentTask.spawn(tab.linkedBrowser, { }, async function () {
    function imgListener(img) {
      return new Promise((resolve, reject) => {
        img.addEventListener("load", () => reject());
        img.addEventListener("error", () => resolve());
      });
    }

    let img = content.document.createElement('img');
    img.src = "http://example.com/tests/image/test/mochitest/shaver.png";
    content.document.body.appendChild(img);

    try {
      await imgListener(img);
      Assert.ok(true);
    } catch (e) {
      Assert.ok(false);
    }

    Assert.equal(img.imageBlockingStatus, Ci.nsIContentPolicy.REJECT_SERVER,
                 "images from http should be blocked");
  });

  gBrowser.removeTab(tab);
});

/**
 * Loading an image from http:// immediately after loading from https://
 * The load from https:// should be replaced.
 */
add_task(async function load_https_and_http_test() {
  let tab = gBrowser.addTab(TEST_URI);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Clear image cache, otherwise in non-e10s mode the image might be cached by
  // previous test, and make the image from https is loaded immediately.
  let imgTools = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools);
  let imageCache = imgTools.getImgCacheForDocument(window.document);
  imageCache.clearCache(false); // false=content

  gBrowser.selectedTab = tab;

  await ContentTask.spawn(tab.linkedBrowser, { }, async function () {
    function imgListener(img) {
      return new Promise((resolve, reject) => {
        img.addEventListener("load", () => reject());
        img.addEventListener("error", () => resolve());
      });
    }

    let img = content.document.createElement("img");
    img.src = "https://example.com/tests/image/test/mochitest/shaver.png";
    img.src = "http://example.com/tests/image/test/mochitest/shaver.png";

    content.document.body.appendChild(img);

    try {
      await imgListener(img);
      Assert.ok(true);
    } catch (e) {
      Assert.ok(false);
    }

    Assert.equal(img.imageBlockingStatus, Ci.nsIContentPolicy.REJECT_SERVER,
                 "image.src changed to http should be blocked");
  });

  gBrowser.removeTab(tab);
});

/**
 * Loading an image from https.
 * Then after we have size information of the image, we immediately change the
 * location to a http:// site (hence should be blocked by CSP). This will make
 * the 2nd request as a PENDING_REQUEST, also blocking 2nd load shouldn't change
 * the imageBlockingStatus value.
 */
add_task(async function block_pending_request_test() {
  let tab = gBrowser.addTab(TEST_URI);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  gBrowser.selectedTab = tab;

  await ContentTask.spawn(tab.linkedBrowser, { }, async function () {
    let wrapper = {
      _resolve: null,
      _sizeAvail: false,

      sizeAvailable(request) {
        // In non-e10s mode the image may have already been cached, so sometimes
        // sizeAvailable() is called earlier then waitUntilSizeAvailable().
        if (this._resolve) {
          this._resolve();
        } else {
          this._sizeAvail = true;
        }
      },

      waitUntilSizeAvailable() {
        return new Promise(resolve => {
          this._resolve = resolve;
          if (this._sizeAvail) {
            resolve();
          }
        });
      },

      QueryInterface(aIID) {
        if (aIID.equals(Ci.imgIScriptedNotificationObserver))
          return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
      }
    };

    let observer = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools)
                     .createScriptedObserver(wrapper);

    let img = content.document.createElement("img");
    img.src = "https://example.com/tests/image/test/mochitest/shaver.png";

    let req = img.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
    img.addObserver(observer);

    content.document.body.appendChild(img);

    info("Wait until Size Available");
    await wrapper.waitUntilSizeAvailable();
    info("Size Available now!");
    img.removeObserver(observer);

    // Now we change to load from http:// site, which will be blocked.
    img.src = "http://example.com/tests/image/test/mochitest/shaver.png";

    Assert.equal(img.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST), req,
                 "CURRENT_REQUEST shouldn't be replaced.");
    Assert.equal(img.imageBlockingStatus, Ci.nsIContentPolicy.ACCEPT);
  });

  gBrowser.removeTab(tab);
});
